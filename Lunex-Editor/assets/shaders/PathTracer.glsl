#version 450 core

// ============================================================================
// PATH TRACER — Compute Shader Progresivo
//
// Fase 4: Software ray tracing via compute shader (OpenGL 4.5+)
//   - Progressive accumulation: 1–N samples per frame
//   - BVH traversal: stack-based sobre SSBO
//   - PBR BRDF: Cook-Torrance con importance sampling GGX
//   - NEE (Next Event Estimation): sampleo directo de luces
//   - Russian Roulette: terminación probabilística de rayos
//   - IBL: mismos cubemaps que el rasterizer
//   - Tone mapping + gamma: ACESFilm idéntico a Mesh3D.glsl
//   - Bindless textures: PBR texture maps via GL_ARB_bindless_texture
//
// Pipeline SPIR-V:
//   GLSL 450 ? shaderc (Vulkan SPIR-V) ? spirv-cross (GLSL 450) ? GL
// ============================================================================

#extension GL_ARB_bindless_texture : enable

#ifdef COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// ============================================================================
// OUTPUT IMAGES
// ============================================================================
layout(rgba32f, binding = 0) uniform image2D u_AccumulationBuffer;
layout(rgba8,   binding = 1) uniform writeonly image2D u_OutputImage;

// ============================================================================
// CAMERA UBO (binding = 15, std140, 192 bytes)
// ============================================================================
layout(std140, binding = 15) uniform CameraData {
    mat4  u_InverseProjection;  // 64   offset 0
    mat4  u_InverseView;        // 64   offset 64
    vec4  u_CameraPosition;     // 16   offset 128
    uint  u_FrameIndex;         // 4    offset 144
    uint  u_SampleCount;        // 4    offset 148
    uint  u_MaxBounces;         // 4    offset 152
    uint  u_SamplesPerFrame;    // 4    offset 156
    uint  u_TriangleCount;      // 4    offset 160
    uint  u_BVHNodeCount;       // 4    offset 164
    uint  u_LightCount;         // 4    offset 168
    uint  u_MaterialCount;      // 4    offset 172
    float u_RussianRoulette;    // 4    offset 176
    float _pad0;                // 4    offset 180
    float _pad1;                // 4    offset 184
    float _pad2;                // 4    offset 188
};

// ============================================================================
// TRIANGLES SSBO (binding = 20, std430)
// RTTriangleGPU: 5 × vec4 = 80 bytes
// ============================================================================
struct RTTriangle {
    vec4 V0;               // xyz=position, w=normal.x
    vec4 V1;               // xyz=position, w=normal.y
    vec4 V2;               // xyz=position, w=normal.z
    vec4 TexCoords01;      // xy=uv0, zw=uv1
    vec4 TexCoords2AndMat; // xy=uv2, z=materialIndex, w=entityID
};

layout(std430, binding = 20) readonly buffer TriangleBuffer {
    RTTriangle u_Triangles[];
};

// ============================================================================
// BVH NODES SSBO (binding = 21, std430)
// RTBVHNodeGPU: 2 × vec4 = 32 bytes
//   BoundsMin.w = leftChild (internal) o triStart (leaf)
//   BoundsMax.w = triCount (0 = internal node)
// ============================================================================
struct BVHNode {
    vec4 BoundsMin; // xyz=aabb min, w=leftChild / triStart
    vec4 BoundsMax; // xyz=aabb max, w=triCount
};

layout(std430, binding = 21) readonly buffer BVHBuffer {
    BVHNode u_BVHNodes[];
};

// ============================================================================
// MATERIALS SSBO (binding = 22, std430)
// RTMaterialGPU: 5 × vec4 = 80 bytes
// ============================================================================
struct RTMaterial {
    vec4 Albedo;           // rgba
    vec4 EmissionAndMeta;  // rgb=emission, a=metallic
    vec4 RoughSpecAO;      // x=roughness, y=specular, z=ao, w=emissionIntensity
    ivec4 TexIndices;      // x=albedo, y=normal, z=metallic, w=roughness  (-1 = none)
    ivec4 TexIndices2;     // x=specular, y=emission, z=ao, w=normalIntensity (floatBitsToInt)
};

layout(std430, binding = 22) readonly buffer MaterialBuffer {
    RTMaterial u_Materials[];
};

// ============================================================================
// LIGHTS SSBO (binding = 23, std430)
// LightData: 5 × vec4 = 80 bytes  (misma struct que Mesh3D.glsl)
// ============================================================================
struct LightData {
    vec4 Position;    // xyz=pos, w=type (0=dir, 1=point, 2=spot)
    vec4 Direction;   // xyz=dir, w=range
    vec4 Color;       // rgb=color, a=intensity
    vec4 Params;      // x=innerCone, y=outerCone, z=castShadows, w=shadowMapIdx
    vec4 Attenuation; // x=constant, y=linear, z=quadratic, w=unused
};

layout(std430, binding = 23) readonly buffer LightBuffer {
    LightData u_Lights[];
};

// ============================================================================
// BINDLESS TEXTURE HANDLES SSBO (binding = 24, std430)
// Each entry is a uint64 handle obtained via glGetTextureHandleARB.
// Materials reference textures by index into this array.
// ============================================================================
layout(std430, binding = 24) readonly buffer TextureHandleBuffer {
    uvec2 u_TextureHandles[];   // uvec2 = 64-bit handle split into low/high
};

// Sample a bindless 2D texture by atlas index.
// Returns vec4(0) if the index is invalid (-1).
vec4 SampleBindlessTexture(int texIndex, vec2 uv) {
#ifdef GL_ARB_bindless_texture
    if (texIndex < 0) return vec4(0.0);
    sampler2D s = sampler2D(u_TextureHandles[texIndex]);
    return texture(s, uv);
#else
    return vec4(0.0);
#endif
}

// ============================================================================
// IBL SAMPLERS (mismos slots que el rasterizer)
// ============================================================================
layout(binding = 8)  uniform samplerCube u_IrradianceMap;
layout(binding = 9)  uniform samplerCube u_PrefilteredMap;
layout(binding = 10) uniform sampler2D   u_BRDFLUT;

// ============================================================================
// CONSTANTS
// ============================================================================
const float PI        = 3.14159265359;
const float TWO_PI    = 6.28318530718;
const float INV_PI    = 0.31830988618;
const float EPSILON   = 1e-6;
const float T_MAX     = 1e30;
const float MIN_ROUGHNESS    = 0.045;
const float MIN_DIELECTRIC_F0 = 0.04;
const float MAX_REFLECTION_LOD = 4.0;
const int   BVH_STACK_SIZE = 64;

// ============================================================================
// RANDOM NUMBER GENERATOR — PCG (Permuted Congruential Generator)
// ============================================================================
uint g_RNGState;

void InitRNG(uvec2 pixel, uint frameIndex) {
    g_RNGState = pixel.x * 1973u + pixel.y * 9277u + frameIndex * 26699u;
    g_RNGState = g_RNGState * 747796405u + 2891336453u;
}

float RandomFloat() {
    g_RNGState = g_RNGState * 747796405u + 2891336453u;
    uint word = ((g_RNGState >> ((g_RNGState >> 28u) + 4u)) ^ g_RNGState) * 277803737u;
    word = (word >> 22u) ^ word;
    return float(word) / 4294967295.0;
}

vec2 RandomVec2() {
    return vec2(RandomFloat(), RandomFloat());
}

vec3 RandomVec3() {
    return vec3(RandomFloat(), RandomFloat(), RandomFloat());
}

// ============================================================================
// SAMPLING UTILITIES
// ============================================================================

// Distribución coseno-ponderada sobre hemisferio (diffuse)
vec3 SampleCosineHemisphere(vec3 N) {
    vec2 xi = RandomVec2();
    float phi = TWO_PI * xi.x;
    float cosTheta = sqrt(xi.y);
    float sinTheta = sqrt(1.0 - xi.y);

    vec3 localDir = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    // Construir TBN desde N
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);

    return normalize(T * localDir.x + B * localDir.y + N * localDir.z);
}

// Importance sampling GGX (specular)
vec3 SampleGGX(vec3 N, float roughness) {
    vec2 xi = RandomVec2();
    float a = roughness * roughness;
    float a2 = a * a;

    float phi = TWO_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a2 - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);

    return normalize(T * H.x + B * H.y + N * H.z);
}

// PDF del GGX importance sampling
float GGX_PDF(float NdotH, float HdotV, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d);
    return (D * NdotH) / max(4.0 * HdotV, EPSILON);
}

// ============================================================================
// PBR BRDF — Cook-Torrance (idéntico a Mesh3D.glsl)
// ============================================================================

float D_GGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + EPSILON);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness) {
    float a2 = roughness * roughness;
    float lambdaV = NdotL * sqrt((-NdotV * a2 + NdotV) * NdotV + a2);
    float lambdaL = NdotV * sqrt((-NdotL * a2 + NdotL) * NdotL + a2);
    return 0.5 / max(lambdaV + lambdaL, EPSILON);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Evaluate full Cook-Torrance BRDF y retorna (diffuse + specular) * NdotL
vec3 EvaluateBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), EPSILON);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL <= 0.0) return vec3(0.0);

    float D = D_GGX(NdotH, roughness);
    float Vis = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
    vec3  F = F_Schlick(VdotH, F0);

    vec3 specular = D * Vis * F;
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo * INV_PI;

    return (diffuse + specular) * NdotL;
}

// ============================================================================
// RAY DEFINITION
// ============================================================================
struct Ray {
    vec3 origin;
    vec3 direction;
    vec3 invDir;
};

Ray MakeRay(vec3 o, vec3 d) {
    Ray r;
    r.origin    = o;
    r.direction = d;
    r.invDir    = 1.0 / max(abs(d), vec3(EPSILON)) * sign(d);
    return r;
}

// ============================================================================
// HIT INFO
// ============================================================================
struct HitInfo {
    float t;
    vec3  position;
    vec3  normal;
    vec2  uv;
    uint  materialIndex;
    int   entityID;
    bool  hit;
};

// ============================================================================
// RAY–TRIANGLE INTERSECTION (Möller–Trumbore)
// ============================================================================
bool IntersectTriangle(Ray ray, RTTriangle tri, out float t, out vec2 barycentrics) {
    vec3 v0 = tri.V0.xyz;
    vec3 v1 = tri.V1.xyz;
    vec3 v2 = tri.V2.xyz;

    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 h  = cross(ray.direction, e2);
    float a = dot(e1, h);

    if (abs(a) < EPSILON) return false;

    float f = 1.0 / a;
    vec3 s = ray.origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;

    vec3 q = cross(s, e1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;

    t = f * dot(e2, q);
    if (t < EPSILON) return false;

    barycentrics = vec2(u, v);
    return true;
}

// ============================================================================
// RAY–AABB INTERSECTION (slab method)
// ============================================================================
bool IntersectAABB(Ray ray, vec3 bmin, vec3 bmax, float tMax) {
    vec3 t0 = (bmin - ray.origin) * ray.invDir;
    vec3 t1 = (bmax - ray.origin) * ray.invDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float enter = max(max(tmin.x, tmin.y), tmin.z);
    float exit  = min(min(tmax.x, tmax.y), tmax.z);
    return enter <= exit && exit >= 0.0 && enter < tMax;
}

// ============================================================================
// BVH TRAVERSAL — Stack-based iterativo
// ============================================================================
HitInfo TraverseBVH(Ray ray) {
    HitInfo bestHit;
    bestHit.t   = T_MAX;
    bestHit.hit = false;

    if (u_TriangleCount == 0u || u_BVHNodeCount == 0u)
        return bestHit;

    uint stack[BVH_STACK_SIZE];
    int stackPtr = 0;
    stack[stackPtr++] = 0u; // root

    while (stackPtr > 0 && stackPtr < BVH_STACK_SIZE) {
        uint nodeIdx = stack[--stackPtr];
        BVHNode node = u_BVHNodes[nodeIdx];

        vec3 bmin = node.BoundsMin.xyz;
        vec3 bmax = node.BoundsMax.xyz;

        if (!IntersectAABB(ray, bmin, bmax, bestHit.t))
            continue;

        // C++ almacena uint como float vía static_cast<float>(value),
        // por lo tanto leemos con uint(float_value), NO floatBitsToUint.
        uint triCount = uint(node.BoundsMax.w);

        if (triCount > 0u) {
            // Nodo hoja: testear triángulos
            uint triStart = uint(node.BoundsMin.w);
            for (uint i = 0u; i < triCount; i++) {
                uint triIdx = triStart + i;
                if (triIdx >= u_TriangleCount) break;

                float t;
                vec2 bary;
                if (IntersectTriangle(ray, u_Triangles[triIdx], t, bary) && t < bestHit.t) {
                    bestHit.t   = t;
                    bestHit.hit = true;

                    RTTriangle tri = u_Triangles[triIdx];
                    float w = 1.0 - bary.x - bary.y;

                    bestHit.position = ray.origin + ray.direction * t;
                    bestHit.normal   = normalize(vec3(tri.V0.w, tri.V1.w, tri.V2.w));

                    // Interpolar UVs con baricéntricas
                    vec2 uv0 = tri.TexCoords01.xy;
                    vec2 uv1 = tri.TexCoords01.zw;
                    vec2 uv2 = tri.TexCoords2AndMat.xy;
                    bestHit.uv = uv0 * w + uv1 * bary.x + uv2 * bary.y;

                    // materialIndex y entityID almacenados como float vía static_cast<float>
                    bestHit.materialIndex = uint(tri.TexCoords2AndMat.z);
                    bestHit.entityID      = int(tri.TexCoords2AndMat.w);
                }
            }
        } else {
            // Nodo interno: push children
            uint leftChild = uint(node.BoundsMin.w);
            uint rightChild = leftChild + 1u;

            if (stackPtr + 2 <= BVH_STACK_SIZE) {
                stack[stackPtr++] = rightChild;
                stack[stackPtr++] = leftChild;
            }
        }
    }

    return bestHit;
}

// ============================================================================
// ATENUACIÓN FÍSICA (igual que Mesh3D.glsl)
// ============================================================================
float GetPhysicalAttenuation(float distance, float range) {
    if (range <= 0.0) return 1.0;
    float atten = 1.0 / max(distance * distance + 0.0001, EPSILON);
    float d = min(distance / range, 1.0);
    float window = pow(max(1.0 - d * d * d * d, 0.0), 2.0);
    return atten * window;
}

float GetSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
    float cosAngle = dot(-L, spotDir);
    float eps = max(innerCone - outerCone, EPSILON);
    float intensity = clamp((cosAngle - outerCone) / eps, 0.0, 1.0);
    return intensity * intensity * (3.0 - 2.0 * intensity);
}

// ============================================================================
// NEXT EVENT ESTIMATION (NEE) — Sampleo directo de luces
// ============================================================================
vec3 SampleDirectLighting(HitInfo hit, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) {
    if (u_LightCount == 0u) return vec3(0.0);

    vec3 Lo = vec3(0.0);

    for (uint i = 0u; i < u_LightCount; i++) {
        LightData light = u_Lights[i];
        int lightType = int(light.Position.w);

        vec3 L;
        float lightDist = T_MAX;
        vec3 radiance;

        if (lightType == 0) {
            // Directional
            L = normalize(-light.Direction.xyz);
            lightDist = T_MAX;
            radiance = light.Color.rgb * light.Color.a;
        }
        else if (lightType == 1) {
            // Point
            vec3 toLight = light.Position.xyz - hit.position;
            lightDist = length(toLight);
            L = toLight / max(lightDist, EPSILON);
            float range = light.Direction.w;
            if (range > 0.0 && lightDist > range) continue;
            float atten = GetPhysicalAttenuation(lightDist, range);
            radiance = light.Color.rgb * light.Color.a * atten;
        }
        else if (lightType == 2) {
            // Spot
            vec3 toLight = light.Position.xyz - hit.position;
            lightDist = length(toLight);
            L = toLight / max(lightDist, EPSILON);
            float range = light.Direction.w;
            if (range > 0.0 && lightDist > range) continue;
            float atten = GetPhysicalAttenuation(lightDist, range);
            vec3 spotDir = normalize(light.Direction.xyz);
            atten *= GetSpotAttenuation(L, spotDir, light.Params.x, light.Params.y);
            if (atten < EPSILON) continue;
            radiance = light.Color.rgb * light.Color.a * atten;
        }
        else {
            continue;
        }

        // Shadow ray: verificar oclusión
        Ray shadowRay = MakeRay(hit.position + hit.normal * EPSILON * 10.0, L);
        HitInfo shadowHit = TraverseBVH(shadowRay);

        bool occluded = shadowHit.hit && shadowHit.t < lightDist - EPSILON;
        if (occluded) continue;

        // Evaluar BRDF × radiancia
        Lo += EvaluateBRDF(hit.normal, V, L, albedo, metallic, roughness, F0) * radiance;
    }

    return Lo;
}

// ============================================================================
// ENVIRONMENT SAMPLING — IBL como fallback para rayos que no impactan
// ============================================================================
vec3 SampleEnvironment(vec3 direction) {
    // Usar el prefiltered map con LOD 0 como environment map
    return textureLod(u_PrefilteredMap, direction, 0.0).rgb;
}

// IBL ambient para un punto de impacto (diffuse + specular indirecto)
vec3 SampleIBLAmbient(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) {
    float NdotV = max(dot(N, V), EPSILON);

    vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo * kD;

    vec3 R = reflect(-V, N);
    float lod = roughness * MAX_REFLECTION_LOD;
    vec3 prefilteredColor = textureLod(u_PrefilteredMap, R, lod).rgb;
    vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    return diffuseIBL + specularIBL;
}

// ============================================================================
// GENERAR RAYO PRIMARIO DESDE CÁMARA
// ============================================================================
Ray GenerateCameraRay(uvec2 pixel, uvec2 screenSize) {
    // Jitter sub-pixel para antialiasing progresivo
    vec2 jitter = RandomVec2() - 0.5;
    vec2 uv = (vec2(pixel) + 0.5 + jitter) / vec2(screenSize);
    uv = uv * 2.0 - 1.0; // [-1, 1]

    // Reconstruir posición en clip space y transformar
    vec4 clipTarget = vec4(uv, 1.0, 1.0);
    vec4 viewTarget = u_InverseProjection * clipTarget;
    viewTarget /= viewTarget.w;

    vec3 worldDir = normalize((u_InverseView * vec4(viewTarget.xyz, 0.0)).xyz);

    return MakeRay(u_CameraPosition.xyz, worldDir);
}

// ============================================================================
// PATH TRACING MAIN LOOP
// ============================================================================
vec3 TracePath(Ray ray) {
    vec3 radiance   = vec3(0.0);
    vec3 throughput = vec3(1.0);

    for (uint bounce = 0u; bounce <= u_MaxBounces; bounce++) {
        HitInfo hit = TraverseBVH(ray);

        if (!hit.hit) {
            // El rayo escapa: samplear environment
            radiance += throughput * SampleEnvironment(ray.direction);
            break;
        }

        // Obtener material
        uint matIdx = min(hit.materialIndex, max(u_MaterialCount, 1u) - 1u);
        RTMaterial mat = u_Materials[matIdx];

        vec3  albedo     = mat.Albedo.rgb;
        float alpha      = mat.Albedo.a;
        vec3  emission   = mat.EmissionAndMeta.rgb * mat.RoughSpecAO.w; // emission * intensity
        float metallic   = mat.EmissionAndMeta.a;
        float roughness  = max(mat.RoughSpecAO.x, MIN_ROUGHNESS);
        float specular   = mat.RoughSpecAO.y;
        float ao         = mat.RoughSpecAO.z;

        // === Texture sampling (bindless) ===
        vec2 uv = hit.uv;

        // Albedo map
        if (mat.TexIndices.x >= 0) {
            vec4 texAlbedo = SampleBindlessTexture(mat.TexIndices.x, uv);
            albedo *= texAlbedo.rgb;
            alpha  *= texAlbedo.a;
        }

        // Metallic map
        if (mat.TexIndices.z >= 0) {
            float texMetal = SampleBindlessTexture(mat.TexIndices.z, uv).r;
            metallic *= texMetal;
        }

        // Roughness map
        if (mat.TexIndices.w >= 0) {
            float texRough = SampleBindlessTexture(mat.TexIndices.w, uv).r;
            roughness = max(roughness * texRough, MIN_ROUGHNESS);
        }

        // Specular map
        if (mat.TexIndices2.x >= 0) {
            float texSpec = SampleBindlessTexture(mat.TexIndices2.x, uv).r;
            specular *= texSpec;
        }

        // Emission map
        if (mat.TexIndices2.y >= 0) {
            vec3 texEmission = SampleBindlessTexture(mat.TexIndices2.y, uv).rgb;
            emission *= texEmission;
        }

        // AO map
        if (mat.TexIndices2.z >= 0) {
            float texAO = SampleBindlessTexture(mat.TexIndices2.z, uv).r;
            ao *= texAO;
        }

        // Normal map (tangent-space perturbation)
        vec3 N = hit.normal;
        if (mat.TexIndices.y >= 0) {
            float normalIntensity = intBitsToFloat(mat.TexIndices2.w);
            vec3 texN = SampleBindlessTexture(mat.TexIndices.y, uv).rgb;
            texN = texN * 2.0 - 1.0;
            texN.xy *= normalIntensity;
            texN = normalize(texN);

            // Build TBN from geometric normal — approximate tangent frame
            vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
            vec3 T = normalize(cross(up, N));
            vec3 B = cross(N, T);
            N = normalize(T * texN.x + B * texN.y + N * texN.z);
        }

        // F0 base
        vec3 F0 = mix(vec3(MIN_DIELECTRIC_F0 * specular), albedo, metallic);

        // Asegurar que la normal apunta hacia el rayo
        if (dot(N, ray.direction) > 0.0) N = -N;

        vec3 V = -ray.direction;

        // === Emisión ===
        radiance += throughput * emission;

        // === NEE: iluminación directa en el primer bounce ===
        // En bounces posteriores también hacemos NEE para convergencia rápida
        HitInfo neeHit = hit;
        neeHit.normal = N;  // use normal-mapped normal for BRDF evaluation
        vec3 directLight = SampleDirectLighting(neeHit, V, albedo, metallic, roughness, F0);
        radiance += throughput * directLight;

        // === IBL ambient en el primer bounce ===
        if (bounce == 0u) {
            vec3 iblAmbient = SampleIBLAmbient(N, V, albedo, metallic, roughness, F0) * ao;
            radiance += throughput * iblAmbient * 0.15; // Reduction since NEE contributes
        }

        // === Russian Roulette (después del segundo bounce) ===
        if (bounce >= 2u) {
            float p = max(throughput.r, max(throughput.g, throughput.b));
            if (p < u_RussianRoulette || RandomFloat() > p) break;
            throughput /= p;
        }

        // === Samplear dirección del siguiente bounce ===
        // Decidir entre diffuse y specular según metallic/roughness
        float specProbability = mix(0.04, 1.0, metallic) * (1.0 - roughness * 0.5);
        specProbability = clamp(specProbability, 0.1, 0.9);

        vec3 newDir;
        float pdf;

        if (RandomFloat() < specProbability) {
            // Specular: importance sample GGX
            vec3 H = SampleGGX(N, roughness);
            newDir = reflect(-V, H);

            if (dot(newDir, N) <= 0.0) break;

            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);
            float NdotL = max(dot(N, newDir), 0.0);
            float NdotV = max(dot(N, V), EPSILON);

            // Cook-Torrance BRDF para el rayo sampleado
            float D   = D_GGX(NdotH, roughness);
            float Vis = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
            vec3  F   = F_Schlick(VdotH, F0);

            // PDF del GGX sampling
            pdf = (D * NdotH) / max(4.0 * VdotH, EPSILON);
            pdf *= specProbability;

            vec3 brdfVal = D * Vis * F;
            throughput *= brdfVal * NdotL / max(pdf, EPSILON);
        } else {
            // Diffuse: cosine-weighted hemisphere
            newDir = SampleCosineHemisphere(N);
            float NdotL = max(dot(N, newDir), 0.0);

            // PDF coseno = NdotL / PI
            pdf = NdotL * INV_PI;
            pdf *= (1.0 - specProbability);

            vec3 F = F_Schlick(max(dot(N, V), 0.0), F0);
            vec3 kD = (1.0 - F) * (1.0 - metallic);
            vec3 brdfVal = kD * albedo * INV_PI;

            throughput *= brdfVal * NdotL / max(pdf, EPSILON);
        }

        // Preparar siguiente rayo (offset para evitar auto-intersección)
        ray = MakeRay(hit.position + N * EPSILON * 10.0, newDir);

        // Safety: si el throughput se descontrola, cortar
        float maxThroughput = max(throughput.r, max(throughput.g, throughput.b));
        if (maxThroughput > 100.0 || isnan(maxThroughput) || isinf(maxThroughput)) break;
    }

    return radiance;
}

// ============================================================================
// TONE MAPPING — ACESFilm (idéntico a Mesh3D.glsl)
// ============================================================================
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ============================================================================
// MAIN — Compute Shader Entry Point
// ============================================================================
void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 screenSize = imageSize(u_AccumulationBuffer);

    // Fuera de la pantalla ? salir
    if (pixel.x >= screenSize.x || pixel.y >= screenSize.y)
        return;

    // Inicializar RNG con posición de pixel + frame index
    InitRNG(uvec2(pixel), u_FrameIndex);

    // Generar rayo primario
    Ray ray = GenerateCameraRay(uvec2(pixel), uvec2(screenSize));

    // Trazar path
    vec3 color = TracePath(ray);

    // Sanitizar (NaN, Inf, negativos)
    color = max(color, vec3(0.0));
    if (any(isnan(color)) || any(isinf(color)))
        color = vec3(0.0);

    // === Acumulación progresiva ===
    vec4 accumulated = imageLoad(u_AccumulationBuffer, pixel);
    accumulated.rgb += color;
    accumulated.a   += 1.0;
    imageStore(u_AccumulationBuffer, pixel, accumulated);

    // === Promedio + tone mapping + gamma ? output final ===
    vec3 averaged = accumulated.rgb / max(accumulated.a, 1.0);

    // Tone mapping ACESFilm
    vec3 mapped = ACESFilm(averaged);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    imageStore(u_OutputImage, pixel, vec4(mapped, 1.0));
}

#endif
