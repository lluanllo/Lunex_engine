#version 450 core

// ============================================================================
// SHADOW RAY TRACING COMPUTE SHADER
// Traces shadow rays using BVH acceleration structure
// ============================================================================

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Triangle {
    vec4 v0;
    vec4 v1;
    vec4 v2;
    vec4 normal;
};

struct BVHNode {
    vec4 aabbMin;
    vec4 aabbMax;
    int firstTriangle;
    int rightChild;
    int parentNode;
    int _padding;
};

struct LightData {
    vec4 Position;   // xyz = position, w = type
    vec4 Direction;  // xyz = direction
    vec4 Color;      // rgb = color, a = intensity
    vec4 Params;     // x = range, y = innerCone, z = outerCone, w = radius
};

struct Ray {
    vec3 origin;
    vec3 direction;
    float tMin;
    float tMax;
};

// ============================================================================
// SHADER STORAGE BUFFERS & TEXTURES
// ============================================================================

layout(std430, binding = 0) readonly buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding = 1) readonly buffer BVHBuffer {
    BVHNode nodes[];
};

layout(std430, binding = 2) readonly buffer LightBuffer {
    int numLights;
    int _padding1;
    int _padding2;
    int _padding3;
    LightData lights[16];
};

// G-Buffer inputs
layout(binding = 0) uniform sampler2D u_GBufferPosition;
layout(binding = 1) uniform sampler2D u_GBufferNormal;
layout(binding = 2) uniform sampler2D u_GBufferDepth;

// Shadow output
layout(rgba16f, binding = 3) uniform writeonly image2D u_ShadowOutput;

// ============================================================================
// UNIFORMS
// ============================================================================

layout(location = 0) uniform int u_TriangleCount;
layout(location = 1) uniform int u_NodeCount;
layout(location = 2) uniform ivec2 u_Resolution;

// Ray tracing settings
layout(location = 3) uniform float u_ShadowRayBias;
layout(location = 4) uniform float u_ShadowSoftness;
layout(location = 5) uniform int u_ShadowSamplesPerLight;

// ============================================================================
// RANDOM NUMBER GENERATION
// ============================================================================

// High-quality hash function for random numbers
float Hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 Hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

// Sample random point on disk (for area light)
vec2 SampleDisk(vec2 uv, float radius) {
    float r = sqrt(uv.x) * radius;
    float theta = uv.y * 2.0 * 3.14159265359;
    return vec2(r * cos(theta), r * sin(theta));
}

// Sample random point on sphere
vec3 SampleSphere(vec2 uv, float radius) {
    float phi = uv.y * 2.0 * 3.14159265359;
    float cosTheta = 1.0 - 2.0 * uv.x;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    return vec3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    ) * radius;
}

// ============================================================================
// RAY-AABB INTERSECTION
// ============================================================================

bool RayAABBIntersect(Ray ray, vec3 boxMin, vec3 boxMax) {
    vec3 invDir = 1.0 / ray.direction;
    vec3 t0 = (boxMin - ray.origin) * invDir;
    vec3 t1 = (boxMax - ray.origin) * invDir;
    
    vec3 tMin = min(t0, t1);
    vec3 tMax = max(t0, t1);
    
    float tNear = max(max(tMin.x, tMin.y), tMin.z);
    float tFar = min(min(tMax.x, tMax.y), tMax.z);
    
    return tNear <= tFar && tFar >= ray.tMin && tNear <= ray.tMax;
}

// ============================================================================
// RAY-TRIANGLE INTERSECTION
// ============================================================================

bool RayTriangleIntersect(Ray ray, Triangle tri, out float t) {
    vec3 edge1 = tri.v1.xyz - tri.v0.xyz;
    vec3 edge2 = tri.v2.xyz - tri.v0.xyz;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    
    if (abs(a) < 1e-6) return false;
    
    float f = 1.0 / a;
    vec3 s = ray.origin - tri.v0.xyz;
    float u = f * dot(s, h);
    
    if (u < 0.0 || u > 1.0) return false;
    
    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    
    if (v < 0.0 || u + v > 1.0) return false;
    
    t = f * dot(edge2, q);
    
    return t > ray.tMin && t < ray.tMax;
}

// ============================================================================
// RAY-PLANE INTERSECTION
// ============================================================================

bool RayPlaneIntersect(Ray ray, float planeY, out float t) {
    // Plane equation: y = planeY (normal = (0, 1, 0))
    float denom = ray.direction.y;
    
    // Ray parallel to plane
    if (abs(denom) < 1e-6) return false;
    
    t = (planeY - ray.origin.y) / denom;
    return t > ray.tMin && t < ray.tMax;
}

// ============================================================================
// BVH TRAVERSAL FOR SHADOW RAYS
// ============================================================================

bool TraceSceneShadowRay(Ray ray) {
    if (u_NodeCount == 0) return false;
    
    // Stack for BVH traversal
    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // Start at root
    
    while (stackPtr > 0) {
        int nodeIndex = stack[--stackPtr];
        if (nodeIndex < 0 || nodeIndex >= u_NodeCount) continue;
        
        BVHNode node = nodes[nodeIndex];
        
        // Test AABB
        if (!RayAABBIntersect(ray, node.aabbMin.xyz, node.aabbMax.xyz))
            continue;
        
        // Check if leaf node (triangleCount stored in aabbMax.w)
        int triangleCount = int(node.aabbMax.w);
        if (triangleCount > 0) {
            // Leaf node - test triangles
            for (int i = 0; i < triangleCount; i++) {
                int triIndex = node.firstTriangle + i;
                if (triIndex < 0 || triIndex >= u_TriangleCount) continue;
                
                float t;
                if (RayTriangleIntersect(ray, triangles[triIndex], t)) {
                    return true; // Hit found - early out for shadows
                }
            }
        }
        else {
            // Internal node - push children to stack
            // leftChild stored in aabbMin.w
            int leftChild = int(node.aabbMin.w);
            int rightChild = node.rightChild;
            
            if (rightChild >= 0 && stackPtr < 32)
                stack[stackPtr++] = rightChild;
            if (leftChild >= 0 && stackPtr < 32)
                stack[stackPtr++] = leftChild;
        }
    }
    
    return false;
}

// ============================================================================
// SHADOW CALCULATION WITH SOFT SHADOWS
// ============================================================================

float CalculateShadow(vec3 fragPos, vec3 normal, LightData light, vec2 seed) {
    vec3 toLight = light.Position.xyz - fragPos;
    float lightDistance = length(toLight);
    vec3 L = normalize(toLight);
    
    // Early out if light behind surface
    if (dot(normal, L) < 0.0) return 0.0;
    
    int lightType = int(light.Position.w);
    float lightRadius = light.Params.w;
    
    // Directional light
    if (lightType == 0) {
        L = normalize(-light.Direction.xyz);
        lightDistance = 1000.0;
        lightRadius *= 0.01; // Scale down for directional lights
    }
    
    // Accumulate shadow samples
    float shadowFactor = 0.0;
    int numSamples = u_ShadowSamplesPerLight;
    
    for (int i = 0; i < numSamples; i++) {
        // Generate jittered sample
        vec2 noise = Hash22(seed + vec2(float(i) * 0.618034, float(i) * 1.32471));
        
        vec3 sampleOffset = vec3(0.0);
        
        if (lightType == 1) {
            // Point light: sample sphere around light
            sampleOffset = SampleSphere(noise, lightRadius * u_ShadowSoftness);
        }
        else if (lightType == 2 || lightType == 0) {
            // Spot/Directional: sample disk perpendicular to light direction
            vec2 diskSample = SampleDisk(noise, lightRadius * u_ShadowSoftness);
            
            // Build orthonormal basis
            vec3 tangent = abs(L.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
            tangent = normalize(cross(tangent, L));
            vec3 bitangent = cross(L, tangent);
            
            sampleOffset = tangent * diskSample.x + bitangent * diskSample.y;
        }
        
        vec3 sampleLightPos = light.Position.xyz + sampleOffset;
        vec3 sampleDir = normalize(sampleLightPos - fragPos);
        
        // Create shadow ray with bias
        Ray shadowRay;
        shadowRay.origin = fragPos + normal * u_ShadowRayBias;
        shadowRay.direction = sampleDir;
        shadowRay.tMin = 0.001;
        shadowRay.tMax = lightDistance;
        
        // Trace shadow ray using BVH
        bool hit = TraceSceneShadowRay(shadowRay);
        
        // Accumulate (if ray hits geometry, it's in shadow)
        shadowFactor += hit ? 0.0 : 1.0;
    }
    
    // Average samples
    shadowFactor /= float(numSamples);
    
    // Apply contact hardening (optional)
    // Shadows become sharper closer to contact point
    if (u_ShadowSoftness > 0.0) {
        float distanceFactor = clamp(lightDistance / 10.0, 0.0, 1.0);
        shadowFactor = mix(shadowFactor * shadowFactor, shadowFactor, distanceFactor);
    }
    
    return shadowFactor;
}

// ============================================================================
// MAIN
// ============================================================================

void main() {
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    if (pixelCoords.x >= u_Resolution.x || pixelCoords.y >= u_Resolution.y)
        return;
    
    // Sample G-Buffer
    vec2 uv = vec2(pixelCoords) / vec2(u_Resolution);
    vec3 fragPos = texture(u_GBufferPosition, uv).rgb;
    vec3 normal = normalize(texture(u_GBufferNormal, uv).rgb);
    float depth = texture(u_GBufferDepth, uv).r;
    
    // Skip background pixels
    if (depth >= 1.0) {
        imageStore(u_ShadowOutput, pixelCoords, vec4(1.0));
        return;
    }
    
    // DEBUG: Visualize BVH node count
    if (u_NodeCount == 0) {
        // Red = no BVH data
        imageStore(u_ShadowOutput, pixelCoords, vec4(1.0, 0.0, 0.0, 1.0));
        return;
    }
    
    // DEBUG: Visualize triangle count
    if (u_TriangleCount == 0) {
        // Blue = no triangle data
        imageStore(u_ShadowOutput, pixelCoords, vec4(0.0, 0.0, 1.0, 1.0));
        return;
    }
    
    // Use pixel coordinates as seed for randomness
    vec2 seed = vec2(pixelCoords) + fract(vec2(u_Resolution) * 0.618034);
    
    // Calculate shadows for all lights
    float shadowFactor = 1.0;
    for (int i = 0; i < numLights && i < 16; i++) {
        float lightShadow = CalculateShadow(fragPos, normal, lights[i], seed);
        shadowFactor = min(shadowFactor, lightShadow);
    }
    
    // Output shadow factor
    imageStore(u_ShadowOutput, pixelCoords, vec4(shadowFactor, shadowFactor, shadowFactor, 1.0));
}
