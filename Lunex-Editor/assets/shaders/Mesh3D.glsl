#version 450 core

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in int a_EntityID;
layout(location = 6) in ivec4 a_BoneIDs;
layout(location = 7) in vec4  a_BoneWeights;

// Bone palette of the mesh being drawn (only read when u_Skinned != 0)
layout(std430, binding = 4) readonly buffer BoneMatrices {
	mat4 u_Bones[];
};

layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
	mat4 u_View;
	mat4 u_Projection;
	vec3 u_ViewPos;
	float _cameraPad;
};

layout(std140, binding = 1) uniform Transform {
	mat4 u_Transform;
	mat4 u_NormalMatrix;   // transpose(inverse(u_Transform)), computed on CPU
	int  u_EntityID;
	int  u_Skinned;
	int  u_DebugBone;      // >= 0: visualize this bone's weights
};

mat4 GetSkinMatrix() {
	if (u_Skinned == 0) return mat4(1.0);
	return a_BoneWeights.x * u_Bones[a_BoneIDs.x] +
	       a_BoneWeights.y * u_Bones[a_BoneIDs.y] +
	       a_BoneWeights.z * u_Bones[a_BoneIDs.z] +
	       a_BoneWeights.w * u_Bones[a_BoneIDs.w];
}

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) out VertexOutput Output;
layout (location = 7) out flat int v_EntityID;
layout (location = 9) out float v_DebugWeight;   // < 0 = debug view off
layout (location = 8) out float v_ViewDepth;

void main() {
	mat4 skin = GetSkinMatrix();
	vec4 worldPos = u_Transform * (skin * vec4(a_Position, 1.0));
	Output.FragPos = worldPos.xyz;
	
	mat3 normalMatrix = mat3(u_NormalMatrix);
	vec3 N = normalize(normalMatrix * (mat3(skin) * a_Normal));
	Output.Normal = N;
	Output.TexCoords = a_TexCoords;
	
	vec3 T = normalize(normalMatrix * (mat3(skin) * a_Tangent));
	T = normalize(T - dot(T, N) * N); // Re-orthogonalize
	vec3 B = cross(N, T) * sign(dot(cross(a_Normal, a_Tangent), a_Bitangent)); // Preserve handedness
	Output.TBN = mat3(T, B, N);
	
	v_EntityID = u_EntityID;

	v_DebugWeight = -1.0;
	if (u_DebugBone >= 0) {
		v_DebugWeight = 0.0;
		for (int k = 0; k < 4; k++)
			if (a_BoneIDs[k] == u_DebugBone) v_DebugWeight += a_BoneWeights[k];
	}
	
	vec4 clipPos = u_ViewProjection * worldPos;
	v_ViewDepth = clipPos.w; // Linear view-space depth for CSM cascade selection
	gl_Position = clipPos;
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;
layout(location = 1) out int o_EntityID;

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) in VertexOutput Input;
layout (location = 7) in flat int v_EntityID;
layout (location = 9) in float v_DebugWeight;

vec3 WeightHeatmap(float w) {
	w = clamp(w, 0.0, 1.0);
	return w < 0.5 ? mix(vec3(0.05, 0.1, 0.9), vec3(0.1, 0.9, 0.2), w * 2.0)
	               : mix(vec3(0.1, 0.9, 0.2), vec3(1.0, 0.1, 0.05), (w - 0.5) * 2.0);
}
layout (location = 8) in float v_ViewDepth;

// ============ MATERIAL PROPERTIES ============
layout(std140, binding = 2) uniform Material {
	vec4 u_Color;
	float u_Metallic;
	float u_Roughness;
	float u_Specular;
	float u_EmissionIntensity;
	vec3 u_EmissionColor;
	float u_NormalIntensity;
	vec3 u_ViewPos;
	
	int u_UseAlbedoMap;
	int u_UseNormalMap;
	int u_UseMetallicMap;
	int u_UseRoughnessMap;
	int u_UseSpecularMap;
	int u_UseEmissionMap;
	int u_UseAOMap;
	float _padding2;
	
	float u_MetallicMultiplier;
	float u_RoughnessMultiplier;
	float u_SpecularMultiplier;
	float u_AOMultiplier;

	// Detail normals & layered texture
	int u_DetailNormalCount;
	int u_UseLayeredTexture;
	int u_LayeredMetallicChannel;   // 0=R, 1=G, 2=B, 3=A
	int u_LayeredRoughnessChannel;

	int u_LayeredAOChannel;
	int u_LayeredUseMetallic;
	int u_LayeredUseRoughness;
	int u_LayeredUseAO;

	// Explicit padding to align vec4 to 16-byte boundary (std140)
	float _detailPad0;

	vec4 u_DetailNormalIntensities;
	vec4 u_DetailNormalTilingX;
	vec4 u_DetailNormalTilingY;

	vec4 u_SubsurfaceColorAmount;   // rgb = scatter color, a = amount
	vec4 u_UVTilingOffset;          // xy = tiling, zw = offset
};

// Material UV transform (tiling 0 = legacy upload without the field -> identity)
vec2 MaterialUV() {
	vec2 tiling = (u_UVTilingOffset.x == 0.0 && u_UVTilingOffset.y == 0.0) ? vec2(1.0) : u_UVTilingOffset.xy;
	return Input.TexCoords * tiling + u_UVTilingOffset.zw;
}


// ============ LIGHTING SYSTEM (SSBO) ============
// Subsurface of the current fragment (material UBO or node graph)
vec4 g_Subsurface = vec4(0.0);

#define MAX_LIGHTS 10000

struct LightData {
	vec4 Position;
	vec4 Direction;
	vec4 Color;
	vec4 Params;
	vec4 Attenuation;
};

layout(std430, binding = 3) buffer Lights {
	int u_NumLights;
	float _padding3;
	float _padding4;
	float _padding5;
	LightData u_Lights[];
};

// ============ SHADOW SYSTEM (UBO binding=7) ============
#define MAX_SHADOW_CASCADES 6
#define MAX_SHADOW_LIGHTS 16

#define SHADOW_TYPE_NONE 0
#define SHADOW_TYPE_DIRECTIONAL 1
#define SHADOW_TYPE_SPOT 2
#define SHADOW_TYPE_POINT 3

struct ShadowCascadeData {
	mat4 ViewProjection;   // 64
	float SplitDepth;      // 4
	float TexelWorldSize; // 4
	float _pad2;           // 4
	float _pad3;           // 4   = 80
};

struct ShadowLightData {
	mat4 ViewProjection[6];  // 384
	vec4 ShadowParams;       // x=bias, y=normalBias, z=firstLayer, w=shadowType
	vec4 ShadowParams2;      // x=range, y=pcfRadius, z=numFaces, w=resolution
};

layout(std140, binding = 7) uniform ShadowData {
	int u_NumShadowLights;
	int u_CSMCascadeCount;
	float u_MaxShadowDistance;
	float u_DistanceSofteningStart;

	float u_DistanceSofteningMax;
	float u_SkyTintStrength;
	int u_ShadowFilterMode;
	float u_ShadowSoftness;
	ShadowCascadeData u_Cascades[MAX_SHADOW_CASCADES];
	ShadowLightData u_Shadows[MAX_SHADOW_LIGHTS];
};

// ============ TEXTURE SAMPLERS ============
#ifndef LNX_MATERIAL_GRAPH
layout (binding = 0) uniform sampler2D u_AlbedoMap;
layout (binding = 1) uniform sampler2D u_NormalMap;
layout (binding = 2) uniform sampler2D u_MetallicMap;
layout (binding = 3) uniform sampler2D u_RoughnessMap;
layout (binding = 4) uniform sampler2D u_SpecularMap;
layout (binding = 5) uniform sampler2D u_EmissionMap;
layout (binding = 6) uniform sampler2D u_AOMap;
layout (binding = 7) uniform sampler2D u_LayeredMap;
#endif

// ============ IBL SAMPLERS ============
layout (binding = 8) uniform samplerCube u_IrradianceMap;
layout (binding = 9) uniform samplerCube u_PrefilteredMap;
layout (binding = 10) uniform sampler2D u_BRDFLUT;

// ============ SHADOW ATLAS SAMPLER ============
layout (binding = 11) uniform sampler2DArrayShadow u_ShadowAtlas;

// ============ DETAIL NORMAL MAP SAMPLERS ============
#ifndef LNX_MATERIAL_GRAPH
layout (binding = 12) uniform sampler2D u_DetailNormal0;
layout (binding = 13) uniform sampler2D u_DetailNormal1;
layout (binding = 14) uniform sampler2D u_DetailNormal2;
layout (binding = 15) uniform sampler2D u_DetailNormal3;
#endif

// ============ IBL UNIFORM ============
layout(std140, binding = 5) uniform IBLData {
	float u_IBLIntensity;
	float u_IBLRotation;
	int u_UseIBL;
	float _iblPadding;
};

// ============ ENHANCED CONSTANTS ============
const float PI = 3.14159265359;
const float EPSILON = 0.00001;
const float MIN_ROUGHNESS = 0.045;
const float MIN_DIELECTRIC_F0 = 0.04;
const float MAX_REFLECTION_LOD = 4.0;

// Light parameters for realistic falloff
const float LIGHT_FALLOFF_BIAS = 0.0001;

// ============ SHADOW SAMPLING FUNCTIONS ============

// Poisson disk for PCF (16 samples)
const vec2 poissonDisk[16] = vec2[](
	vec2(-0.94201624, -0.39906216),
	vec2( 0.94558609, -0.76890725),
	vec2(-0.09418410, -0.92938870),
	vec2( 0.34495938,  0.29387760),
	vec2(-0.91588581,  0.45771432),
	vec2(-0.81544232, -0.87912464),
	vec2(-0.38277543,  0.27676845),
	vec2( 0.97484398,  0.75648379),
	vec2( 0.44323325, -0.97511554),
	vec2( 0.53742981, -0.47373420),
	vec2(-0.26496911, -0.41893023),
	vec2( 0.79197514,  0.19090188),
	vec2(-0.24188840,  0.99706507),
	vec2(-0.81409955,  0.91437590),
	vec2( 0.19984126,  0.78641367),
	vec2( 0.14383161, -0.14100790)
);

// Single-sample shadow comparison
float SampleShadow(float layer, vec2 uv, float compareDepth) {
	return texture(u_ShadowAtlas, vec4(uv, layer, compareDepth));
}

// PCF shadow with Poisson disk sampling
float ShadowNoise(vec2 p) {
	return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// Hard: one hardware-filtered tap (2x2 bilinear compare).
// Soft: 16-tap Poisson disk rotated per pixel - the random rotation turns the
// stepped "layers" of a fixed kernel into fine noise that reads as a smooth penumbra.
float SampleShadowPCF(float layer, vec2 uv, float compareDepth, float radius, float resolution) {
	if (u_ShadowFilterMode == 0 || radius <= 0.0) {
		return texture(u_ShadowAtlas, vec4(uv, layer, compareDepth));
	}
	float texelSize = 1.0 / resolution;
	float angle = ShadowNoise(gl_FragCoord.xy) * 6.2831853;
	float s = sin(angle), c = cos(angle);
	mat2 rot = mat2(c, s, -s, c);
	float shadow = 0.0;
	for (int i = 0; i < 16; i++) {
		vec2 offset = rot * poissonDisk[i] * radius * texelSize;
		shadow += texture(u_ShadowAtlas, vec4(uv + offset, layer, compareDepth));
	}
	return shadow / 16.0;
}

// Calculate shadow for a directional light (CSM)
// One cascade lookup. Bias and filter size are expressed in world units through the
// cascade's texel size, so the shadow looks the same on both sides of a cascade split.
float SampleCascade(ShadowLightData sd, int cascade, vec3 fragPos, vec3 normal) {
	float texelWorld = max(u_Cascades[cascade].TexelWorldSize, 1e-4);
	vec3 biasedPos = fragPos + normal * max(sd.ShadowParams.y, texelWorld * 1.5);
	vec4 lightSpacePos = sd.ViewProjection[cascade] * vec4(biasedPos, 1.0);
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w * 0.5 + 0.5;
	if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0)
		return 1.0;

	float compareDepth = clamp(projCoords.z - sd.ShadowParams.x * 0.25, 0.0, 1.0);
	float radiusTexels = clamp(u_ShadowSoftness / texelWorld, 1.0, 12.0);
	return SampleShadowPCF(sd.ShadowParams.z + float(cascade), projCoords.xy, compareDepth, radiusTexels, sd.ShadowParams2.w);
}

float CalculateDirectionalShadowAtDepth(int shadowIndex, vec3 fragPos, vec3 normal, float viewDepth) {
	if (shadowIndex < 0 || shadowIndex >= u_NumShadowLights) return 1.0;
	if (viewDepth > u_MaxShadowDistance) return 1.0;

	ShadowLightData sd = u_Shadows[shadowIndex];
	int count = clamp(u_CSMCascadeCount, 1, MAX_SHADOW_CASCADES);

	int cascade = count - 1;
	for (int c = 0; c < count; c++) {
		if (viewDepth < u_Cascades[c].SplitDepth) { cascade = c; break; }
	}

	float shadow = SampleCascade(sd, cascade, fragPos, normal);

	// Blend into the next cascade over the last 15% of this one (no visible seam)
	if (cascade < count - 1) {
		float start = (cascade == 0) ? 0.0 : u_Cascades[cascade - 1].SplitDepth;
		float end = u_Cascades[cascade].SplitDepth;
		float blendStart = mix(start, end, 0.85);
		float t = clamp((viewDepth - blendStart) / max(end - blendStart, 1e-4), 0.0, 1.0);
		if (t > 0.0) shadow = mix(shadow, SampleCascade(sd, cascade + 1, fragPos, normal), t);
	}

	// Fade out towards the max shadow distance instead of cutting off
	float fadeStart = u_MaxShadowDistance * 0.85;
	shadow = mix(shadow, 1.0, clamp((viewDepth - fadeStart) / max(u_MaxShadowDistance - fadeStart, 1e-4), 0.0, 1.0));
	return shadow;
}

float CalculateDirectionalShadow(int shadowIndex, vec3 fragPos, vec3 normal) {
	return CalculateDirectionalShadowAtDepth(shadowIndex, fragPos, normal, v_ViewDepth);
}

// Calculate shadow for a spot light
float CalculateSpotShadow(int shadowIndex, vec3 fragPos, vec3 normal) {
	if (shadowIndex < 0 || shadowIndex >= u_NumShadowLights) return 1.0;
	
	ShadowLightData sd = u_Shadows[shadowIndex];
	float layer = sd.ShadowParams.z;
	float bias = sd.ShadowParams.x;
	float normalBias = sd.ShadowParams.y;
	float pcfRadius = sd.ShadowParams2.y;
	float resolution = sd.ShadowParams2.w;
	
	vec3 biasedPos = fragPos + normal * normalBias;
	vec4 lightSpacePos = sd.ViewProjection[0] * vec4(biasedPos, 1.0);
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5;
	
	if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
		projCoords.y < 0.0 || projCoords.y > 1.0 ||
		projCoords.z > 1.0) {
		return 1.0;
	}
	
	float compareDepth = projCoords.z - bias;
	return SampleShadowPCF(layer, projCoords.xy, compareDepth, pcfRadius, resolution);
}

// Calculate shadow for a point light (cubemap stored as 6 layers)
float CalculatePointShadow(int shadowIndex, vec3 fragPos, vec3 normal, vec3 lightPos) {
	if (shadowIndex < 0 || shadowIndex >= u_NumShadowLights) return 1.0;
	
	ShadowLightData sd = u_Shadows[shadowIndex];
	float firstLayer = sd.ShadowParams.z;
	float bias = sd.ShadowParams.x;
	float normalBias = sd.ShadowParams.y;
	float lightRange = sd.ShadowParams2.x;
	float resolution = sd.ShadowParams2.w;
	
	vec3 fragToLight = fragPos - lightPos;
	float currentDist = length(fragToLight);
	
	// Skip shadow if fragment is beyond light range (keep very close fragments)
	if (currentDist > lightRange) return 1.0;
	
	// Apply normal bias scaled by distance to light
	float normalBiasScale = max(normalBias, normalBias * (currentDist / lightRange));
	vec3 biasedFrag = fragPos + normal * normalBiasScale;
	float biasedDist = length(biasedFrag - lightPos);
	
	// Determine which cubemap face to sample
	vec3 absDir = abs(fragToLight);
	int face;
	if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
		face = fragToLight.x > 0.0 ? 0 : 1;  // +X or -X
	} else if (absDir.y >= absDir.x && absDir.y >= absDir.z) {
		face = fragToLight.y > 0.0 ? 2 : 3;  // +Y or -Y
	} else {
		face = fragToLight.z > 0.0 ? 4 : 5;  // +Z or -Z
	}
	
	// Transform through the face's view-projection
	vec4 lightSpacePos = sd.ViewProjection[face] * vec4(biasedFrag, 1.0);
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5;
	
	if (projCoords.z > 1.0 || projCoords.z < 0.0) return 1.0;
	if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
		projCoords.y < 0.0 || projCoords.y > 1.0) return 1.0;
	
	// For point lights, compare linear depth (written by ShadowDepthPoint shader)
	float normalizedDist = biasedDist / lightRange;
	
	// Adaptive bias: smaller bias when close, larger when far
	// This prevents both near-plane artifacts and far-plane clipping
	float adaptiveBias = bias * mix(0.2, 1.5, currentDist / lightRange);
	float compareDepth = normalizedDist - adaptiveBias;
	
	// Clamp to valid range
	compareDepth = clamp(compareDepth, 0.0, 1.0);
	
	float layer = firstLayer + float(face);
	
	// Use smaller PCF radius for point lights (they already have 6 face transitions)
	float pointPCFRadius = max(0.5, 1.0 * (1.0 - currentDist / lightRange));
	return SampleShadowPCF(layer, projCoords.xy, compareDepth, pointPCFRadius, resolution);
}

// Find the shadow entry owned by the light at 'lightIndex' (ShadowParams2.z = light index)
int FindShadowForLight(int lightIndex) {
	for (int s = 0; s < u_NumShadowLights && s < MAX_SHADOW_LIGHTS; s++) {
		if (int(u_Shadows[s].ShadowParams2.z + 0.5) == lightIndex) return s;
	}
	return -1;
}

// ============ ADVANCED PBR FUNCTIONS ============

// Optimized GGX with numerical stability improvements
float D_GGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	
	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return nom / max(denom, EPSILON);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	
	float lambdaV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float lambdaL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	
	return 0.5 / max(lambdaV + lambdaL, EPSILON);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
	float f = pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
	return F0 + (1.0 - F0) * f;
}

vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============ ADVANCED DIFFUSE MODELS ============

vec3 Diffuse_Lambert(vec3 albedo) {
	return albedo / PI;
}

float Diffuse_Burley(float NdotV, float NdotL, float LdotH, float roughness) {
	float f90 = 0.5 + 2.0 * LdotH * LdotH * roughness;
	float lightScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotL, 5.0);
	float viewScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotV, 5.0);
	return lightScatter * viewScatter / PI;
}

float Diffuse_OrenNayar(float NdotV, float NdotL, float LdotV, float roughness) {
	float s = LdotV - NdotL * NdotV;
	float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));
	
	float sigma2 = roughness * roughness;
	float A = 1.0 - 0.5 * (sigma2 / (sigma2 + 0.33));
	float B = 0.45 * sigma2 / (sigma2 + 0.09);
	
	return max(0.0, NdotL) * (A + B * s / t) / PI;
}

// ============ PHYSICALLY-BASED LIGHT ATTENUATION ============

float GetPhysicalAttenuation(float distance, float range) {
	if (range <= 0.0) return 1.0;
	
	float attenuation = 1.0 / max(distance * distance + LIGHT_FALLOFF_BIAS, EPSILON);
	
	float distRatio = min(distance / range, 1.0);
	float window = pow(max(1.0 - pow(distRatio, 4.0), 0.0), 2.0);
	
	return attenuation * window;
}

float GetUE4Attenuation(float distance, float radius) {
	float d = distance / max(radius, EPSILON);
	float d2 = d * d;
	float d4 = d2 * d2;
	
	float falloff = clamp(1.0 - d4, 0.0, 1.0);
	falloff = falloff * falloff;
	
	return falloff / (distance * distance + 1.0);
}

float GetSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
	float cosAngle = dot(-L, spotDir);
	float epsilon = max(innerCone - outerCone, EPSILON);
	float intensity = clamp((cosAngle - outerCone) / epsilon, 0.0, 1.0);
	
	return intensity * intensity * (3.0 - 2.0 * intensity);
}

// ============ REALISTIC AMBIENT OCCLUSION ============

float GetMultiBounceAO(float ao, vec3 albedo) {
	vec3 a = 2.0 * albedo - 0.33;
	vec3 b = -4.8 * albedo + 0.64;
	vec3 c = 2.75 * albedo + 0.69;
	
	float x = ao;
	vec3 aoMultiBounce = max(vec3(x), ((x * a + b) * x + c) * x);
	
	return dot(aoMultiBounce, vec3(0.333));
}

float GetDirectionalAO(float ao, vec3 N, vec3 bentNormal) {
	float bentFactor = max(dot(N, bentNormal), 0.0);
	return mix(ao, 1.0, bentFactor * 0.5);
}

// ============ ADVANCED LIGHTING FEATURES ============

float GetSoftShadow(vec3 L, float NdotL, float lightRadius) {
	float softness = lightRadius * (1.0 - NdotL);
	return smoothstep(-softness, softness, NdotL);
}

vec3 GetLightIntensity(vec3 color, float intensity, float distance, float radius) {
	float energyScale = 1.0 / max(radius * radius, 1.0);
	return color * intensity * energyScale;
}

// ============ MAIN LIGHTING CALCULATION (WITH SHADOWS) ============

// Output: directional shadow factor for IBL modulation
float g_DirectionalShadowFactor = 1.0;

// ============ COAT / SHEEN (Principled BSDF layers) ============
float g_CoatWeight = 0.0, g_CoatRoughness = 0.03, g_SheenWeight = 0.0, g_SheenRoughness = 0.5;
vec3 g_CoatTint = vec3(1.0), g_SheenTint = vec3(1.0), g_SpecularTint = vec3(1.0);

// Charlie sheen distribution + Neubelt visibility (Estevez & Kulla)
float D_Charlie(float roughness, float NdotH) {
	float a = max(roughness * roughness, 1e-3);
	float invR = 1.0 / a;
	float sin2h = max(1.0 - NdotH * NdotH, 0.0078125);
	return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * PI);
}
float V_Neubelt(float NdotV, float NdotL) {
	return clamp(1.0 / (4.0 * (NdotL + NdotV - NdotL * NdotV)), 0.0, 1.0);
}

// Direct light: sheen on top of the base, then a clear coat (GGX, IOR 1.5) that attenuates what is below
vec3 LayerLobes(vec3 base, float NdotV, float NdotL, float NdotH, float VdotH) {
	vec3 result = base;
	if (g_SheenWeight > 0.0) {
		vec3 sheen = g_SheenTint * g_SheenWeight * D_Charlie(g_SheenRoughness, NdotH) * V_Neubelt(NdotV, NdotL);
		result = result * (1.0 - g_SheenWeight * 0.2) + sheen;
	}
	if (g_CoatWeight > 0.0) {
		float cr = max(g_CoatRoughness, 0.03);
		float Fc = F_Schlick(VdotH, vec3(0.04)).x * g_CoatWeight;
		float coat = D_GGX(NdotH, cr) * V_SmithGGXCorrelated(NdotV, NdotL, cr) * Fc;
		result = result * (1.0 - Fc) * mix(vec3(1.0), g_CoatTint, g_CoatWeight) + vec3(coat);
	}
	return result;
}

// Environment light for the same layers
vec3 LayerAmbient(vec3 base, vec3 N, vec3 V, float ao) {
	if (g_SheenWeight <= 0.0 && g_CoatWeight <= 0.0) return base;
	float NdotV = max(dot(N, V), 1e-4);
	float cosR = cos(u_IBLRotation), sinR = sin(u_IBLRotation);
	vec3 result = base;
	if (g_SheenWeight > 0.0) {
		vec3 rn = vec3(cosR * N.x - sinR * N.z, N.y, sinR * N.x + cosR * N.z);
		vec3 irr = (u_UseIBL != 0) ? texture(u_IrradianceMap, rn).rgb * u_IBLIntensity : vec3(0.35);
		result = result * (1.0 - g_SheenWeight * 0.2) + irr * g_SheenTint * g_SheenWeight * 0.2 * ao;
	}
	if (g_CoatWeight > 0.0) {
		float cr = max(g_CoatRoughness, 0.03);
		float Fc = (0.04 + 0.96 * pow(1.0 - NdotV, 5.0)) * g_CoatWeight;
		vec3 R = reflect(-V, N);
		vec3 rr = vec3(cosR * R.x - sinR * R.z, R.y, sinR * R.x + cosR * R.z);
		vec3 env = (u_UseIBL != 0) ? textureLod(u_PrefilteredMap, rr, cr * MAX_REFLECTION_LOD).rgb * u_IBLIntensity : vec3(0.35);
		result = result * (1.0 - Fc) * mix(vec3(1.0), g_CoatTint, g_CoatWeight) + env * Fc * ao;
	}
	return result;
}

// ============ SUBSURFACE SCATTERING (approximate) ============
// Wrap diffuse lets light bleed past the terminator; the transmission term
// (Barre-Brisebois/Bouchard "fast translucency") lights thin parts from behind.
const float SSS_WRAP = 0.5;
const float SSS_DISTORTION = 0.25;
const float SSS_POWER = 3.0;
const float SSS_SCALE = 1.5;

vec3 SubsurfaceLighting(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 sssColor, float sssAmount,
                        vec3 radiance, float shadowFactor) {
	float NdotLraw = dot(N, L);
	float wrap = clamp((NdotLraw + SSS_WRAP) / (1.0 + SSS_WRAP), 0.0, 1.0);
	// extra diffuse beyond plain Lambert, tinted by the scatter color
	vec3 wrapTerm = sssColor * albedo * max(wrap - max(NdotLraw, 0.0), 0.0) / PI * shadowFactor;

	vec3 Ht = normalize(L + N * SSS_DISTORTION);
	float back = pow(clamp(dot(V, -Ht), 0.0, 1.0), SSS_POWER) * SSS_SCALE;
	// shadows only partially block transmitted light (it travels through the object)
	vec3 transmission = sssColor * albedo * back * mix(0.35, 1.0, shadowFactor);

	return (wrapTerm + transmission) * radiance * sssAmount;
}

vec3 CalculateDirectLighting(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, vec3 fragPos, float ao) {
	vec3 Lo = vec3(0.0);
	float NdotV = max(dot(N, V), EPSILON);
	
	int lightCount = min(u_NumLights, MAX_LIGHTS);
	
	// Reset directional shadow factor
	g_DirectionalShadowFactor = 1.0;
	
	// Track shadow index per type for matching
	int nextDirShadow = 0;
	int nextSpotShadow = 0;
	int nextPointShadow = 0;

	
	for (int i = 0; i < lightCount; i++) {
		LightData light = u_Lights[i];
		int lightType = int(light.Position.w);
		
		vec3 L;
		vec3 radiance;
		float attenuation = 1.0;
		float lightRadius = 0.1;
		
		// Shadow factor - default 1.0 (fully lit, no shadow)
		float shadowFactor = 1.0;
		bool castsShadows = light.Params.z > 0.5;
		
		// ========== LIGHT TYPE PROCESSING =========
		if (lightType == 0) {
			// Directional Light
			L = normalize(-light.Direction.xyz);
			radiance = light.Color.rgb * light.Color.a;
			lightRadius = 0.0;
			
			// Shadow
			if (castsShadows) {
				shadowFactor = CalculateDirectionalShadow(FindShadowForLight(i), fragPos, N);
				// Store for IBL ambient modulation (first directional shadow wins)
				g_DirectionalShadowFactor = min(g_DirectionalShadowFactor, shadowFactor);
			}
			
		} else if (lightType == 1) {
			// Point Light
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / max(distance, EPSILON);
			
			float range = light.Direction.w;
			if (range > 0.0 && distance > range) {
				continue;
			}
			
			attenuation = GetPhysicalAttenuation(distance, range);
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.05, 0.05);
			
			// Shadow
			if (castsShadows) {
				shadowFactor = CalculatePointShadow(FindShadowForLight(i), fragPos, N, light.Position.xyz);
			}
			
		} else if (lightType == 2) {
			// Spot Light
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / max(distance, EPSILON);
			
			float range = light.Direction.w;
			if (range > 0.0 && distance > range) {
				continue;
			}
			
			attenuation = GetPhysicalAttenuation(distance, range);
			
			vec3 spotDir = normalize(light.Direction.xyz);
			float spotAtten = GetSpotAttenuation(L, spotDir, light.Params.x, light.Params.y);
			attenuation *= spotAtten;
			
			if (attenuation < EPSILON) {
				continue;
			}
			
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.08, 0.08);
			
			// Shadow
			if (castsShadows) {
				shadowFactor = CalculateSpotShadow(FindShadowForLight(i), fragPos, N);
			}
		}
		
		if (attenuation < EPSILON) continue;

		if (g_Subsurface.a > 0.0) {
			Lo += SubsurfaceLighting(N, V, L, albedo * (1.0 - metallic), g_Subsurface.rgb, g_Subsurface.a,
			                         radiance * attenuation, shadowFactor);
		}
		
		// ========== BRDF EVALUATION ==========
		vec3 H = normalize(V + L);
		
		float NdotL = max(dot(N, L), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float LdotH = max(dot(L, H), 0.0);
		float LdotV = max(dot(L, V), 0.0);
		
		if (NdotL < EPSILON) continue;
		
		// Combine analytical soft shadow with real shadow map
		float softFactor = GetSoftShadow(L, NdotL, lightRadius);
		shadowFactor *= softFactor;
		
		// ========== SPECULAR TERM (Cook-Torrance) ==========
		float D = D_GGX(NdotH, roughness);
		float V_term = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
		vec3 F = F_Schlick(VdotH, F0);
		
		vec3 specular = D * V_term * F;
		
		// ========== DIFFUSE TERM (Burley for realism) ==========
		vec3 kS = F;
		vec3 kD = (1.0 - kS) * (1.0 - metallic);
		
		float burley = Diffuse_Burley(NdotV, NdotL, LdotH, roughness);
		vec3 diffuse = kD * albedo * burley;
		
		// ========== MICRO-OCCLUSION ==========
		float microOcclusion = mix(1.0, ao, 0.5);
		
		// ========== ACCUMULATE LIGHTING (shadow applied to radiance) ==========
		radiance *= attenuation * shadowFactor;
		Lo += LayerLobes(diffuse + specular, NdotV, NdotL, NdotH, VdotH) * radiance * NdotL * microOcclusion;
	}
	
	return Lo;
}

// ============ IBL AMBIENT LIGHTING ============

vec3 CalculateIBLAmbient(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
	float NdotV = max(dot(N, V), EPSILON);
	
	vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - metallic);
	
	float cosR = cos(u_IBLRotation);
	float sinR = sin(u_IBLRotation);
	vec3 rotatedN = vec3(
		cosR * N.x - sinR * N.z,
		N.y,
		sinR * N.x + cosR * N.z
	);
	
	vec3 irradiance = texture(u_IrradianceMap, rotatedN).rgb;
	vec3 diffuseIBL = irradiance * albedo * kD;
	
	vec3 R = reflect(-V, N);
	vec3 rotatedR = vec3(
		cosR * R.x - sinR * R.z,
		R.y,
		sinR * R.x + cosR * R.z
	);
	
	float lod = roughness * MAX_REFLECTION_LOD;
	vec3 prefilteredColor = textureLod(u_PrefilteredMap, rotatedR, lod).rgb;
	
	vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
	vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
	
	vec3 ambient = (diffuseIBL + specularIBL) * u_IBLIntensity * ao;
	
	return ambient;
}

// ============ FALLBACK AMBIENT (No IBL) ============

vec3 CalculateFallbackAmbient(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
	float NdotV = max(dot(N, V), EPSILON);
	
	vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - metallic);
	
	float skyFactor = N.y * 0.5 + 0.5;
	vec3 skyColor = vec3(0.4, 0.5, 0.6) * 0.6;
	vec3 groundColor = vec3(0.3, 0.25, 0.2) * 0.2;
	vec3 hemisphereLight = mix(groundColor, skyColor, skyFactor);
	
	vec3 ambientDiffuse = albedo * kD * hemisphereLight * ao;
	
	vec3 R = reflect(-V, N);
	float horizonFade = pow(max(R.y, 0.0), 0.5);
	vec3 ambientSpecular = F * mix(groundColor, skyColor, horizonFade) * (1.0 - roughness) * ao * 0.1;
	
	return (ambientDiffuse + ambientSpecular) * 1.2;
}

// ============ HELPER: SAMPLE CHANNEL FROM LAYERED TEXTURE ============

float SampleLayeredChannel(vec4 layeredSample, int channel) {
	if (channel == 0) return layeredSample.r;
	if (channel == 1) return layeredSample.g;
	if (channel == 2) return layeredSample.b;
	return layeredSample.a;
}

// ============ HELPER: BLEND DETAIL NORMALS (UDN blending) ============

vec3 BlendNormals_UDN(vec3 base, vec3 detail) {
	return normalize(vec3(base.xy + detail.xy, base.z));
}

vec3 SampleDetailNormal(sampler2D detailMap, vec2 uv, float tilingX, float tilingY) {
	vec2 detailUV = uv * vec2(tilingX, tilingY);
	vec3 n = texture(detailMap, detailUV).rgb;
	return n * 2.0 - 1.0;
}

// ============ TONE MAPPING ============

vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

#ifdef LNX_MATERIAL_GRAPH
// ============ NODE GRAPH MATERIAL (code generated by MaterialGraphCompiler) ============
#define LNX_UV Input.TexCoords
#define LNX_POS Input.FragPos
#define LNX_NORMAL normalize(Input.Normal)
#define LNX_TBN Input.TBN
#define LNX_VIEWPOS u_ViewPos

layout(std140, binding = 11) uniform GraphParams {
	vec4 u_GraphParams[512];   // [0].x = time
};
layout (binding = 0) uniform sampler2D u_GraphTex0;
layout (binding = 1) uniform sampler2D u_GraphTex1;
layout (binding = 2) uniform sampler2D u_GraphTex2;
layout (binding = 3) uniform sampler2D u_GraphTex3;
layout (binding = 4) uniform sampler2D u_GraphTex4;
layout (binding = 5) uniform sampler2D u_GraphTex5;
layout (binding = 6) uniform sampler2D u_GraphTex6;
layout (binding = 7) uniform sampler2D u_GraphTex7;
layout (binding = 12) uniform sampler2D u_GraphTex8;
layout (binding = 13) uniform sampler2D u_GraphTex9;
layout (binding = 14) uniform sampler2D u_GraphTex10;
layout (binding = 15) uniform sampler2D u_GraphTex11;

// Engine data for the node graph (same blocks as the vertex stage; instanced to avoid name clashes)
layout(std140, binding = 0) uniform Camera { mat4 u_ViewProjection; mat4 u_View; mat4 u_Projection; vec3 u_ViewPos; float _cameraPad; } lnx_cam;
layout(std140, binding = 1) uniform Transform { mat4 u_Transform; mat4 u_NormalMatrix; int u_EntityID; int u_Skinned; int u_DebugBone; } lnx_xf;
#define LNX_VIEW lnx_cam.u_View
#define LNX_VIEWPROJ lnx_cam.u_ViewProjection
#define LNX_MODEL lnx_xf.u_Transform
#define LNX_MODEL_INV transpose(lnx_xf.u_NormalMatrix)
#define LNX_OBJECT_ID float(lnx_xf.u_EntityID)
#define LNX_OBJPOS (LNX_MODEL_INV * vec4(LNX_POS, 1.0)).xyz
#ifdef LNX_FORWARD_HDR
layout (binding = 16) uniform sampler2D u_SceneColor;   // opaque scene (mipmapped) for refraction
#endif

// Shader to RGB: full lighting of a closure (forward pass)
vec3 lnx_light(vec3 albedo, float metallic, float roughness, float specular, vec3 N, float ao) {
	vec3 V = normalize(u_ViewPos - Input.FragPos);
	roughness = max(roughness, MIN_ROUGHNESS);
	vec3 F0 = mix(vec3(MIN_DIELECTRIC_F0 * specular * 2.0), albedo, metallic);
	vec3 c = CalculateDirectLighting(N, V, F0, albedo, metallic, roughness, Input.FragPos, ao);
	c += (u_UseIBL != 0) ? CalculateIBLAmbient(N, V, F0, albedo, metallic, roughness, ao) : CalculateFallbackAmbient(N, V, F0, albedo, metallic, roughness, ao);
	return c;
}

// @LNX_GRAPH_CODE@
#endif

void main() {
#ifdef LNX_MATERIAL_GRAPH
	LnxBSDF gm = lnx_evaluate_graph();
#ifndef LNX_FORWARD_HDR
	if (gm.Alpha < 0.5) discard;   // previews: alpha clip
#endif
	float metallic = clamp(gm.Metallic, 0.0, 1.0);
	float transmission = clamp(gm.Transmission, 0.0, 1.0) * (1.0 - metallic);
	vec3 baseColor = max(gm.BaseColor, vec3(0.0));     // node colors are linear (image nodes decode sRGB)
	vec3 albedo = baseColor * (1.0 - transmission);    // transmitted light replaces the diffuse part
	float alpha = clamp(gm.Alpha, 0.0, 1.0);
	vec3 N = normalize(gm.Normal);
	float roughness = max(clamp(gm.Roughness, 0.0, 1.0), MIN_ROUGHNESS);
	float specular = max(gm.Specular, 0.0);
	float ao = clamp(gm.AO, 0.0, 1.0);
	g_Subsurface = gm.Subsurface;
	g_CoatWeight = clamp(gm.Coat, 0.0, 1.0); g_CoatRoughness = gm.CoatRoughness; g_CoatTint = gm.CoatTint;
	g_SheenWeight = clamp(gm.Sheen, 0.0, 1.0); g_SheenRoughness = gm.SheenRoughness; g_SheenTint = gm.SheenTint;
	g_SpecularTint = gm.SpecularTint;
#else
	// ========== TEXTURE SAMPLING ==========
	
	vec3 albedo;
	float alpha = u_Color.a;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, MaterialUV());
		albedo = pow(texColor.rgb, vec3(2.2));
		alpha *= texColor.a;
	} else {
		albedo = pow(u_Color.rgb, vec3(2.2));
	}
	
	// ========== NORMAL MAPPING ==========
	vec3 N;
	if (u_UseNormalMap != 0) {
		vec3 normalMap = texture(u_NormalMap, MaterialUV()).rgb;
		normalMap = normalMap * 2.0 - 1.0;
		normalMap.xy *= u_NormalIntensity;
		normalMap = normalize(normalMap);
		
		// Blend detail normals (UDN method)
		if (u_DetailNormalCount > 0) {
			vec3 detailN;
			detailN = SampleDetailNormal(u_DetailNormal0, MaterialUV(), u_DetailNormalTilingX.x, u_DetailNormalTilingY.x);
			detailN.xy *= u_DetailNormalIntensities.x;
			normalMap = BlendNormals_UDN(normalMap, detailN);
		}
		if (u_DetailNormalCount > 1) {
			vec3 detailN;
			detailN = SampleDetailNormal(u_DetailNormal1, MaterialUV(), u_DetailNormalTilingX.y, u_DetailNormalTilingY.y);
			detailN.xy *= u_DetailNormalIntensities.y;
			normalMap = BlendNormals_UDN(normalMap, detailN);
		}
		if (u_DetailNormalCount > 2) {
			vec3 detailN;
			detailN = SampleDetailNormal(u_DetailNormal2, MaterialUV(), u_DetailNormalTilingX.z, u_DetailNormalTilingY.z);
			detailN.xy *= u_DetailNormalIntensities.z;
			normalMap = BlendNormals_UDN(normalMap, detailN);
		}
		if (u_DetailNormalCount > 3) {
			vec3 detailN;
			detailN = SampleDetailNormal(u_DetailNormal3, MaterialUV(), u_DetailNormalTilingX.w, u_DetailNormalTilingY.w);
			detailN.xy *= u_DetailNormalIntensities.w;
			normalMap = BlendNormals_UDN(normalMap, detailN);
		}
		
		N = normalize(Input.TBN * normalMap);
	} else {
		N = normalize(Input.Normal);
		
		// Even without a base normal map, detail normals can be used
		if (u_DetailNormalCount > 0) {
			vec3 detailAccum = vec3(0.0, 0.0, 1.0);
			if (u_DetailNormalCount > 0) {
				vec3 d = SampleDetailNormal(u_DetailNormal0, MaterialUV(), u_DetailNormalTilingX.x, u_DetailNormalTilingY.x);
				d.xy *= u_DetailNormalIntensities.x;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			if (u_DetailNormalCount > 1) {
				vec3 d = SampleDetailNormal(u_DetailNormal1, MaterialUV(), u_DetailNormalTilingX.y, u_DetailNormalTilingY.y);
				d.xy *= u_DetailNormalIntensities.y;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			if (u_DetailNormalCount > 2) {
				vec3 d = SampleDetailNormal(u_DetailNormal2, MaterialUV(), u_DetailNormalTilingX.z, u_DetailNormalTilingY.z);
				d.xy *= u_DetailNormalIntensities.z;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			if (u_DetailNormalCount > 3) {
				vec3 d = SampleDetailNormal(u_DetailNormal3, MaterialUV(), u_DetailNormalTilingX.w, u_DetailNormalTilingY.w);
				d.xy *= u_DetailNormalIntensities.w;
				detailAccum = BlendNormals_UDN(detailAccum, d);
			}
			N = normalize(Input.TBN * detailAccum);
		}
	}
	
	// ========== LAYERED TEXTURE SAMPLING ==========
	vec4 layeredSample = vec4(0.0);
	if (u_UseLayeredTexture != 0) {
		layeredSample = texture(u_LayeredMap, MaterialUV());
	}
	
	// ========== METALLIC ==========
	float metallic = u_Metallic;
	if (u_UseLayeredTexture != 0 && u_LayeredUseMetallic != 0) {
		metallic = clamp(SampleLayeredChannel(layeredSample, u_LayeredMetallicChannel) * u_MetallicMultiplier, 0.0, 1.0);
	} else if (u_UseMetallicMap != 0) {
		metallic = clamp(texture(u_MetallicMap, MaterialUV()).r * u_MetallicMultiplier, 0.0, 1.0);
	}
	
	// ========== ROUGHNESS ==========
	float roughness = u_Roughness;
	if (u_UseLayeredTexture != 0 && u_LayeredUseRoughness != 0) {
		roughness = clamp(SampleLayeredChannel(layeredSample, u_LayeredRoughnessChannel) * u_RoughnessMultiplier, 0.0, 1.0);
	} else if (u_UseRoughnessMap != 0) {
		roughness = clamp(texture(u_RoughnessMap, MaterialUV()).r * u_RoughnessMultiplier, 0.0, 1.0);
	}
	roughness = max(roughness, MIN_ROUGHNESS);
	
	float specular = u_Specular;
	if (u_UseSpecularMap != 0) {
		specular = clamp(texture(u_SpecularMap, MaterialUV()).r * u_SpecularMultiplier, 0.0, 1.0);
	}
	
	// ========== AMBIENT OCCLUSION ==========
	float ao = 1.0;
	if (u_UseLayeredTexture != 0 && u_LayeredUseAO != 0) {
		ao = clamp(SampleLayeredChannel(layeredSample, u_LayeredAOChannel) * u_AOMultiplier, 0.0, 1.0);
	} else if (u_UseAOMap != 0) {
		ao = clamp(texture(u_AOMap, MaterialUV()).r * u_AOMultiplier, 0.0, 1.0);
	}
	
	g_Subsurface = u_SubsurfaceColorAmount;
#endif // LNX_MATERIAL_GRAPH

	// ========== PBR LIGHTING ==========
	
	vec3 V = normalize(u_ViewPos - Input.FragPos);
	
	vec3 F0 = vec3(MIN_DIELECTRIC_F0 * specular * 2.0);
	F0 = mix(F0, albedo, metallic);
#ifdef LNX_MATERIAL_GRAPH
	F0 = mix(vec3(MIN_DIELECTRIC_F0 * specular * 2.0) * g_SpecularTint, baseColor, metallic);
#endif
	
	vec3 directLighting = CalculateDirectLighting(N, V, F0, albedo, metallic, roughness, Input.FragPos, ao);
	
	vec3 ambient;
	if (u_UseIBL != 0) {
		ambient = CalculateIBLAmbient(N, V, F0, albedo, metallic, roughness, ao);
	} else {
		ambient = CalculateFallbackAmbient(N, V, F0, albedo, metallic, roughness, ao);
	}
	ambient = LayerAmbient(ambient, N, V, ao);
	
	// ========== EMISSION ==========
	
	vec3 emission = vec3(0.0);
	float emissiveLuminance = 0.0;
	
#ifdef LNX_MATERIAL_GRAPH
	emission = max(gm.Emission, vec3(0.0));
	emissiveLuminance = dot(emission, vec3(0.2126, 0.7152, 0.0722));
#else
	if (u_EmissionIntensity > EPSILON) {
		emission = u_EmissionColor * u_EmissionIntensity;
		if (u_UseEmissionMap != 0) {
			vec3 emissionTex = texture(u_EmissionMap, MaterialUV()).rgb;
			emissionTex = pow(emissionTex, vec3(2.2));
			emission = emissionTex * u_EmissionColor * u_EmissionIntensity;
		}
		
		emissiveLuminance = dot(emission, vec3(0.2126, 0.7152, 0.0722));
	}
	
#endif
	// ========== EMISSIVE CONTRIBUTION ==========
	
	vec3 emissiveContribution = vec3(0.0);
	if (emissiveLuminance > EPSILON) {
		vec3 emissiveLight = emission * 0.3;
		float upwardBias = max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0) * 0.5 + 0.5;
		float kD = (1.0 - metallic);
		emissiveContribution = albedo * emissiveLight * kD * upwardBias * ao;
	}
	
	// ========== FINAL COMPOSITION ==========
	
	if (u_NumShadowLights > 0 && g_DirectionalShadowFactor < 1.0) {
		float ambientShadow = mix(0.35, 1.0, g_DirectionalShadowFactor);
		ambient *= ambientShadow;
	}
	
	vec3 color = ambient + directLighting + emission + emissiveContribution;
	
	// Sky color tinting on shadowed areas
	if (u_SkyTintStrength > 0.0 && u_NumShadowLights > 0) {
		vec3 skyColor;
		if (u_UseIBL != 0) {
			skyColor = texture(u_IrradianceMap, vec3(0.0, 1.0, 0.0)).rgb;
		} else {
			skyColor = vec3(0.4, 0.5, 0.7);
		}
		
		float luminanceDirect = dot(directLighting, vec3(0.2126, 0.7152, 0.0722));
		float luminanceAmbient = dot(ambient, vec3(0.2126, 0.7152, 0.0722));
		float shadowAmount = 1.0 - clamp(luminanceDirect / max(luminanceAmbient + luminanceDirect, 0.001), 0.0, 1.0);
		
		vec3 skyTint = skyColor * u_SkyTintStrength * shadowAmount * albedo;
		color += skyTint;
	}
	
	// ========== TRANSMISSION (glass / refraction / transparent BSDF) ==========
#ifdef LNX_MATERIAL_GRAPH
	if (transmission > 0.0) {
		vec3 dir = gm.Refract > 0.5 ? refract(-V, N, 1.0 / max(gm.IOR, 1.0001)) : -V;
		if (dot(dir, dir) < 1e-6) dir = reflect(-V, N);   // total internal reflection
		vec3 Fv = F_Schlick(max(dot(N, V), 0.0), F0);
#ifdef LNX_FORWARD_HDR
		// Screen-space refraction: the opaque scene seen through the surface, blurred by roughness
		vec4 clip = lnx_cam.u_ViewProjection * vec4(Input.FragPos + dir * 0.35, 1.0);
		vec2 uv = clamp(clip.xy / max(clip.w, 1e-4) * 0.5 + 0.5, vec2(0.001), vec2(0.999));
		vec3 behind = textureLod(u_SceneColor, uv, roughness * 6.0).rgb;
#else
		// Previews: refract the environment
		vec3 behind = (u_UseIBL != 0) ? textureLod(u_PrefilteredMap, dir, roughness * MAX_REFLECTION_LOD).rgb * u_IBLIntensity : vec3(0.5);
#endif
		color += behind * baseColor * transmission * (1.0 - Fv);
	}
#endif

#ifdef LNX_FORWARD_HDR
	// Transparent / forward pass: linear HDR, premultiplied alpha (blend ONE, ONE_MINUS_SRC_ALPHA)
	if (v_DebugWeight >= 0.0) { FragColor = vec4(WeightHeatmap(v_DebugWeight), 1.0); o_EntityID = v_EntityID; return; }
	FragColor = vec4(color * alpha, alpha);
	o_EntityID = v_EntityID;
#else
	// Tone mapping
	color = ACESFilm(color);

	// Gamma correction
	color = pow(color, vec3(1.0 / 2.2));

	if (v_DebugWeight >= 0.0) { FragColor = vec4(WeightHeatmap(v_DebugWeight), 1.0); o_EntityID = v_EntityID; return; }
	FragColor = vec4(color, alpha);
	o_EntityID = v_EntityID;
#endif
}

#endif