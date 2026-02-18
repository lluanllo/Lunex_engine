#version 450 core

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in int a_EntityID;

layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
};

layout(std140, binding = 1) uniform Transform {
	mat4 u_Transform;
};

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) out VertexOutput Output;
layout (location = 7) out flat int v_EntityID;
layout (location = 8) out float v_ViewDepth;

void main() {
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	Output.FragPos = worldPos.xyz;
	
	mat3 normalMatrix = mat3(transpose(inverse(u_Transform)));
	vec3 N = normalize(normalMatrix * a_Normal);
	Output.Normal = N;
	Output.TexCoords = a_TexCoords;
	
	vec3 rawT = normalMatrix * a_Tangent;
	
	// Handle degenerate tangents (e.g. sphere poles where tangent is zero)
	if (dot(rawT, rawT) < 0.0001) {
		// Generate a fallback tangent perpendicular to the normal
		vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
		rawT = cross(N, up);
	}
	
	vec3 T = normalize(rawT);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);
	Output.TBN = mat3(T, B, N);
	
	v_EntityID = a_EntityID;
	
	vec4 clipPos = u_ViewProjection * worldPos;
	v_ViewDepth = clipPos.w;
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
layout (location = 8) in float v_ViewDepth;

// ============ MATERIAL PROPERTIES ============
layout(std140, binding = 2) uniform Material {
	vec4 u_Color;                         // 16
	float u_Metallic;                     // 4
	float u_Roughness;                    // 4
	float u_Specular;                     // 4
	float u_EmissionIntensity;            // 4  = 32
	vec3 u_EmissionColor;                 // 12
	float u_NormalIntensity;              // 4  = 48
	
	int u_UseAlbedoMap;                   // 4
	int u_UseNormalMap;                   // 4
	int u_UseMetallicMap;                 // 4
	int u_UseRoughnessMap;                // 4  = 64
	int u_UseSpecularMap;                 // 4
	int u_UseEmissionMap;                 // 4
	int u_UseAOMap;                       // 4
	int u_UseLayeredMap;                  // 4  = 80
	
	float u_MetallicMultiplier;           // 4
	float u_RoughnessMultiplier;          // 4
	float u_SpecularMultiplier;           // 4
	float u_AOMultiplier;                 // 4  = 96

	vec2 u_UVTiling;                      // 8
	vec2 u_UVOffset;                      // 8  = 112

	int u_LayeredChannelMetallic;         // 4
	int u_LayeredChannelRoughness;        // 4
	int u_LayeredChannelAO;               // 4
	int u_UseHeightMap;                   // 4  = 128

	float u_HeightScale;                  // 4
	int u_UseDetailNormalMap;             // 4
	float u_DetailNormalScale;            // 4
	float u_AlphaCutoff;                  // 4  = 144

	vec2 u_DetailUVTiling;                // 8
	int u_AlphaMode;                      // 4
	int u_FlipNormalMapY;                 // 4  = 160

	int u_AlbedoColorSpace;               // 4
	int u_NormalColorSpace;               // 4
	int u_LayeredColorSpace;              // 4
	int u_EmissionColorSpace;             // 4  = 176
};

// ============ LIGHTING SYSTEM (SSBO) ============
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
	float _lightPad1;
	float _lightPad2;
	float _lightPad3;
	LightData u_Lights[];
};

// ============ SHADOW SYSTEM (UBO binding=7) ============
#define MAX_SHADOW_CASCADES 4
#define MAX_SHADOW_LIGHTS 16

#define SHADOW_TYPE_NONE 0
#define SHADOW_TYPE_DIRECTIONAL 1
#define SHADOW_TYPE_SPOT 2
#define SHADOW_TYPE_POINT 3

struct ShadowCascadeData {
	mat4 ViewProjection;   // 64
	float SplitDepth;      // 4
	float _pad1;           // 4
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
	float _shadowPad1;
	float _shadowPad2;
	ShadowCascadeData u_Cascades[MAX_SHADOW_CASCADES];
	ShadowLightData u_Shadows[MAX_SHADOW_LIGHTS];
};

// ============ TEXTURE SAMPLERS ============
layout (binding = 0) uniform sampler2D u_AlbedoMap;
layout (binding = 1) uniform sampler2D u_NormalMap;
layout (binding = 2) uniform sampler2D u_MetallicMap;
layout (binding = 3) uniform sampler2D u_RoughnessMap;
layout (binding = 4) uniform sampler2D u_SpecularMap;
layout (binding = 5) uniform sampler2D u_EmissionMap;
layout (binding = 6) uniform sampler2D u_AOMap;
layout (binding = 7) uniform sampler2D u_LayeredMap;

// ============ IBL SAMPLERS ============
layout (binding = 8) uniform samplerCube u_IrradianceMap;
layout (binding = 9) uniform samplerCube u_PrefilteredMap;
layout (binding = 10) uniform sampler2D u_BRDFLUT;

// ============ SHADOW ATLAS SAMPLER ============
layout (binding = 11) uniform sampler2DArrayShadow u_ShadowAtlas;

// ============ NEW TEXTURE SAMPLERS ============
layout (binding = 12) uniform sampler2D u_HeightMap;
layout (binding = 13) uniform sampler2D u_DetailNormalMap;

// ============ IBL UNIFORM ============
layout(std140, binding = 5) uniform IBLData {
	float u_IBLIntensity;
	float u_IBLRotation;
	int u_UseIBL;
	float _iblPadding;
};

// ============ VIEW POSITION UNIFORM ============
layout(std140, binding = 4) uniform ViewPosition {
	vec3 u_ViewPos;
	float _viewPosPadding;
};

// ============ ENHANCED CONSTANTS ============
const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float INV_PI = 0.31830988618;
const float EPSILON = 0.00001;
const float MIN_ROUGHNESS = 0.045;
const float MIN_DIELECTRIC_F0 = 0.04;
const float MAX_REFLECTION_LOD = 4.0;

const float LIGHT_FALLOFF_BIAS = 0.0001;

// ============ SHADOW SAMPLING FUNCTIONS ============

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

// Vogel disk - better distribution than Poisson for variable-radius PCF / PCSS
vec2 VogelDiskSample(int sampleIndex, int sampleCount, float phi) {
	float goldenAngle = 2.4;
	float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(sampleCount));
	float theta = float(sampleIndex) * goldenAngle + phi;
	return vec2(cos(theta), sin(theta)) * r;
}

// Interleaved gradient noise for per-pixel sample rotation (reduces banding)
float InterleavedGradientNoise(vec2 screenPos) {
	vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

float SampleShadow(float layer, vec2 uv, float compareDepth) {
	return texture(u_ShadowAtlas, vec4(uv, layer, compareDepth));
}

// Improved PCF with Vogel disk + per-pixel rotation to break up banding
float SampleShadowPCF(float layer, vec2 uv, float compareDepth, float radius, float resolution) {
	float texelSize = 1.0 / resolution;
	float shadow = 0.0;
	float phi = InterleavedGradientNoise(gl_FragCoord.xy) * TWO_PI;
	
	const int SAMPLE_COUNT = 16;
	for (int i = 0; i < SAMPLE_COUNT; i++) {
		vec2 offset = VogelDiskSample(i, SAMPLE_COUNT, phi) * radius * texelSize;
		shadow += texture(u_ShadowAtlas, vec4(uv + offset, layer, compareDepth));
	}
	
	return shadow / float(SAMPLE_COUNT);
}

// PCSS-style blocker search for contact-hardening shadows
float FindBlockerDistance(float layer, vec2 uv, float receiverDepth, float searchRadius, float resolution) {
	float texelSize = 1.0 / resolution;
	float phi = InterleavedGradientNoise(gl_FragCoord.xy) * TWO_PI;
	
	float blockerSum = 0.0;
	int blockerCount = 0;
	
	const int BLOCKER_SAMPLES = 8;
	for (int i = 0; i < BLOCKER_SAMPLES; i++) {
		vec2 offset = VogelDiskSample(i, BLOCKER_SAMPLES, phi) * searchRadius * texelSize;
		// Sample the shadow map without comparison to get raw depth
		float sampleResult = texture(u_ShadowAtlas, vec4(uv + offset, layer, receiverDepth));
		if (sampleResult < 0.5) {
			// This sample is blocked - estimate blocker at roughly the receiver depth
			blockerSum += receiverDepth;
			blockerCount++;
		}
	}
	
	if (blockerCount == 0) return -1.0; // No blockers found
	return blockerSum / float(blockerCount);
}

// Contact-hardening PCF: wider penumbra for distant shadows, sharp for contact
float SampleShadowPCSS(float layer, vec2 uv, float compareDepth, float basePCFRadius, float resolution, float lightSize) {
	// Step 1: Blocker search
	float searchRadius = basePCFRadius * 2.0;
	float blockerDepth = FindBlockerDistance(layer, uv, compareDepth, searchRadius, resolution);
	
	if (blockerDepth < 0.0) return 1.0; // No blockers = fully lit
	
	// Step 2: Penumbra estimation
	float penumbraWidth = max(compareDepth - blockerDepth, 0.0) / max(blockerDepth, EPSILON) * lightSize;
	float filterRadius = max(penumbraWidth * basePCFRadius, basePCFRadius * 0.5);
	filterRadius = min(filterRadius, basePCFRadius * 4.0); // Clamp max spread
	
	// Step 3: PCF with variable radius
	return SampleShadowPCF(layer, uv, compareDepth, filterRadius, resolution);
}

// Calculate shadow for a directional light (CSM) with PCSS
float CalculateDirectionalShadow(int shadowIndex, vec3 fragPos, vec3 normal) {
	if (shadowIndex < 0 || shadowIndex >= u_NumShadowLights) return 1.0;
	
	ShadowLightData sd = u_Shadows[shadowIndex];
	float firstLayer = sd.ShadowParams.z;
	float bias = sd.ShadowParams.x;
	float normalBias = sd.ShadowParams.y;
	float pcfRadius = sd.ShadowParams2.y;
	float resolution = sd.ShadowParams2.w;
	
	int cascadeIndex = u_CSMCascadeCount - 1;
	for (int c = 0; c < u_CSMCascadeCount; c++) {
		if (v_ViewDepth < u_Cascades[c].SplitDepth) {
			cascadeIndex = c;
			break;
		}
	}
	
	// Scale normal bias by cascade to reduce peter-panning on far cascades
	float cascadeScale = 1.0 + float(cascadeIndex) * 0.5;
	vec3 biasedPos = fragPos + normal * normalBias * cascadeScale;
	vec4 lightSpacePos = u_Cascades[cascadeIndex].ViewProjection * vec4(biasedPos, 1.0);
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5;
	
	if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
		projCoords.y < 0.0 || projCoords.y > 1.0 ||
		projCoords.z > 1.0) {
		return 1.0;
	}
	
	float compareDepth = projCoords.z - bias;
	float layer = firstLayer + float(cascadeIndex);
	
	// Use PCSS for contact-hardening on close cascades, regular PCF on far ones
	float shadow;
	if (cascadeIndex <= 1) {
		float lightSize = 1.0 + float(cascadeIndex) * 0.5;
		shadow = SampleShadowPCSS(layer, projCoords.xy, compareDepth, pcfRadius, resolution, lightSize);
	} else {
		shadow = SampleShadowPCF(layer, projCoords.xy, compareDepth, pcfRadius, resolution);
	}
	
	// Fade out at max shadow distance
	float fadeStart = u_MaxShadowDistance * 0.8;
	float fadeFactor = clamp((u_MaxShadowDistance - v_ViewDepth) / (u_MaxShadowDistance - fadeStart), 0.0, 1.0);
	shadow = mix(1.0, shadow, fadeFactor);
	
	// Cascade transition blending
	if (cascadeIndex < u_CSMCascadeCount - 1) {
		float cascadeFar = u_Cascades[cascadeIndex].SplitDepth;
		float blendRange = cascadeFar * 0.1;
		float blendFactor = clamp((cascadeFar - v_ViewDepth) / blendRange, 0.0, 1.0);
		
		if (blendFactor < 1.0) {
			int nextCascade = cascadeIndex + 1;
			float nextCascadeScale = 1.0 + float(nextCascade) * 0.5;
			vec3 nextBiasedPos = fragPos + normal * normalBias * nextCascadeScale;
			vec4 nextLightSpace = u_Cascades[nextCascade].ViewProjection * vec4(nextBiasedPos, 1.0);
			vec3 nextProj = nextLightSpace.xyz / nextLightSpace.w;
			nextProj = nextProj * 0.5 + 0.5;
			
			float nextLayer = firstLayer + float(nextCascade);
			float nextCompare = nextProj.z - bias;
			float nextShadow = SampleShadowPCF(nextLayer, nextProj.xy, nextCompare, pcfRadius, resolution);
			
			shadow = mix(nextShadow, shadow, blendFactor);
		}
	}
	
	return shadow;
}

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
	return SampleShadowPCSS(layer, projCoords.xy, compareDepth, pcfRadius, resolution, 1.0);
}

float CalculatePointShadow(int shadowIndex, vec3 fragPos, vec3 normal, vec3 lightPos) {
	if (shadowIndex < 0 || shadowIndex >= u_NumShadowLights) return 1.0;
	
	ShadowLightData sd = u_Shadows[shadowIndex];
	float firstLayer = sd.ShadowParams.z;
	float bias = sd.ShadowParams.x;
	float normalBias = sd.ShadowParams.y;
	float lightRange = sd.ShadowParams2.x;
	float resolution = sd.ShadowParams2.w;
	
	vec3 fragToLight = fragPos - lightPos;
	vec3 biasedFrag = fragPos + normal * normalBias;
	float currentDist = length(biasedFrag - lightPos);
	
	vec3 absDir = abs(fragToLight);
	int face;
	if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
		face = fragToLight.x > 0.0 ? 0 : 1;
	} else if (absDir.y >= absDir.x && absDir.y >= absDir.z) {
		face = fragToLight.y > 0.0 ? 2 : 3;
	} else {
		face = fragToLight.z > 0.0 ? 4 : 5;
	}
	
	vec4 lightSpacePos = sd.ViewProjection[face] * vec4(biasedFrag, 1.0);
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5;
	
	if (projCoords.z > 1.0) return 1.0;
	
	float normalizedDist = currentDist / lightRange;
	float compareDepth = normalizedDist - bias;
	
	float layer = firstLayer + float(face);
	return SampleShadowPCF(layer, projCoords.xy, compareDepth, 1.0, resolution);
}

int FindShadowIndex(int lightType, vec3 lightPos, vec3 lightDir) {
	for (int s = 0; s < u_NumShadowLights; s++) {
		int sType = int(u_Shadows[s].ShadowParams.w);
		if (sType == lightType) {
			if (lightType == SHADOW_TYPE_DIRECTIONAL) return s;
			return s;
		}
	}
	return -1;
}

// ============ ADVANCED PBR FUNCTIONS ============

// GGX/Trowbridge-Reitz NDF
float D_GGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return a2 / max(denom, EPSILON);
}

// Height-correlated Smith GGX visibility term
float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	
	float lambdaV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float lambdaL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	
	return 0.5 / max(lambdaV + lambdaL, EPSILON);
}

// Fast approximation for mobile/perf (Hammon 2017)
float V_SmithGGXCorrelatedFast(float NdotV, float NdotL, float roughness) {
	float a = roughness;
	float GGXV = NdotL * (NdotV * (1.0 - a) + a);
	float GGXL = NdotV * (NdotL * (1.0 - a) + a);
	return 0.5 / max(GGXV + GGXL, EPSILON);
}

// Schlick Fresnel with configurable f90 (energy-conserving)
vec3 F_Schlick(float cosTheta, vec3 F0, float f90) {
	return F0 + (vec3(f90) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
	return F_Schlick(cosTheta, F0, 1.0);
}

vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============ SPECULAR ANTI-ALIASING ============
// Reduces specular shimmer/aliasing on distant surfaces (Kaplanyan 2016 / Filament)

float SpecularAntiAliasing(vec3 N, float roughness) {
	// Compute screen-space derivatives of the normal
	vec3 dndu = dFdx(N);
	vec3 dndv = dFdy(N);
	
	// Estimate the variance of the normal distribution
	float variance = max(dot(dndu, dndu), dot(dndv, dndv));
	
	// Project roughness threshold
	float kernelRoughness = min(2.0 * variance, 0.18);
	
	// Increase roughness to reduce aliased highlights
	return clamp(sqrt(roughness * roughness + kernelRoughness), 0.0, 1.0);
}

// ============ MULTI-SCATTERING ENERGY COMPENSATION ============
// Kulla-Conty approximation: compensates for energy lost in single-scatter BRDF
// at high roughness + high F0 (metals look too dark without this)

vec3 EnergyCompensation(vec3 F0, vec2 dfg) {
	// dfg.x = scale, dfg.y = bias from BRDF LUT
	// Single-scatter energy: F0 * dfg.x + dfg.y
	vec3 Ess = F0 * dfg.x + vec3(dfg.y);
	// Multi-scatter energy compensation factor
	// Derived from: 1 + F0 * (1/(Ess) - 1) simplified
	return 1.0 + F0 * (1.0 / max(Ess, vec3(EPSILON)) - 1.0);
}

// ============ SPECULAR OCCLUSION FROM AO ============
// Lagarde & de Rousiers 2014 - derives specular occlusion from diffuse AO

float ComputeSpecularAO(float NdotV, float ao, float roughness) {
	return clamp(pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

// ============ MULTI-BOUNCE AO ============
// GTR approximation for multi-bounce AO (Jimenez 2016)

float GetMultiBounceAO(float ao, vec3 albedo) {
	vec3 a = 2.0404 * albedo - 0.3324;
	vec3 b = -4.7951 * albedo + 0.6417;
	vec3 c = 2.7552 * albedo + 0.6903;
	
	float x = ao;
	vec3 aoMultiBounce = max(vec3(x), ((x * a + b) * x + c) * x);
	
	return dot(aoMultiBounce, vec3(0.333));
}

// ============ DIFFUSE MODELS ============

vec3 Diffuse_Lambert(vec3 albedo) {
	return albedo * INV_PI;
}

// Disney/Burley diffuse with energy-conserving f90
float Diffuse_Burley(float NdotV, float NdotL, float LdotH, float roughness) {
	float f90 = 0.5 + 2.0 * LdotH * LdotH * roughness;
	float lightScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotL, 5.0);
	float viewScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotV, 5.0);
	return lightScatter * viewScatter * INV_PI;
}

float Diffuse_OrenNayar(float NdotV, float NdotL, float LdotV, float roughness) {
	float s = LdotV - NdotL * NdotV;
	float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));
	
	float sigma2 = roughness * roughness;
	float A = 1.0 - 0.5 * (sigma2 / (sigma2 + 0.33));
	float B = 0.45 * sigma2 / (sigma2 + 0.09);
	
	return max(0.0, NdotL) * (A + B * s / t) * INV_PI;
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

// ============ ADVANCED LIGHTING FEATURES ============

float GetSoftShadow(vec3 L, float NdotL, float lightRadius) {
	float softness = lightRadius * (1.0 - NdotL);
	return smoothstep(-softness, softness, NdotL);
}

vec3 GetLightIntensity(vec3 color, float intensity, float distance, float radius) {
	float energyScale = 1.0 / max(radius * radius, 1.0);
	return color * intensity * energyScale;
}

float GetDirectionalAO(float ao, vec3 N, vec3 bentNormal) {
	float bentFactor = max(dot(N, bentNormal), 0.0);
	return mix(ao, 1.0, bentFactor * 0.5);
}

// ============ MAIN LIGHTING CALCULATION (WITH SHADOWS) ============

float g_DirectionalShadowFactor = 1.0;

vec3 CalculateDirectLighting(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, vec3 fragPos, float ao) {
	vec3 Lo = vec3(0.0);
	float NdotV = max(dot(N, V), EPSILON);
	
	// Compute energy-conserving f90 for dielectrics (Filament model)
	float f90 = clamp(50.0 * dot(F0, vec3(0.33)), 0.0, 1.0);
	
	int lightCount = min(u_NumLights, MAX_LIGHTS);
	
	g_DirectionalShadowFactor = 1.0;
	
	int nextDirShadow = 0;
	int nextSpotShadow = 0;
	int nextPointShadow = 0;

	int dirShadowIndices[MAX_SHADOW_LIGHTS];
	int spotShadowIndices[MAX_SHADOW_LIGHTS];
	int pointShadowIndices[MAX_SHADOW_LIGHTS];
	int numDirShadows = 0;
	int numSpotShadows = 0;
	int numPointShadows = 0;

	for (int s = 0; s < u_NumShadowLights && s < MAX_SHADOW_LIGHTS; s++) {
		int sType = int(u_Shadows[s].ShadowParams.w);
		if (sType == SHADOW_TYPE_DIRECTIONAL && numDirShadows < MAX_SHADOW_LIGHTS)
			dirShadowIndices[numDirShadows++] = s;
		else if (sType == SHADOW_TYPE_SPOT && numSpotShadows < MAX_SHADOW_LIGHTS)
			spotShadowIndices[numSpotShadows++] = s;
		else if (sType == SHADOW_TYPE_POINT && numPointShadows < MAX_SHADOW_LIGHTS)
			pointShadowIndices[numPointShadows++] = s;
	}

	int dirCounter = 0, spotCounter = 0, pointCounter = 0;
	
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
			if (castsShadows && dirCounter < numDirShadows) {
				shadowFactor = CalculateDirectionalShadow(dirShadowIndices[dirCounter], fragPos, N);
				dirCounter++;
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
				if (castsShadows) pointCounter++;
				continue;
			}
			
			attenuation = GetPhysicalAttenuation(distance, range);
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.05, 0.05);
			
			// Shadow
			if (castsShadows && pointCounter < numPointShadows) {
				shadowFactor = CalculatePointShadow(pointShadowIndices[pointCounter], fragPos, N, light.Position.xyz);
				pointCounter++;
			}
			
		} else if (lightType == 2) {
			// Spot Light
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / max(distance, EPSILON);
			
			float range = light.Direction.w;
			if (range > 0.0 && distance > range) {
				if (castsShadows) spotCounter++;
				continue;
			}
			
			attenuation = GetPhysicalAttenuation(distance, range);
			
			vec3 spotDir = normalize(light.Direction.xyz);
			float spotAtten = GetSpotAttenuation(L, spotDir, light.Params.x, light.Params.y);
			attenuation *= spotAtten;
			
			if (attenuation < EPSILON) {
				if (castsShadows) spotCounter++;
				continue;
			}
			
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.08, 0.08);
			
			// Shadow
			if (castsShadows && spotCounter < numSpotShadows) {
				shadowFactor = CalculateSpotShadow(spotShadowIndices[spotCounter], fragPos, N);
				spotCounter++;
			}
		}
		
		if (attenuation < EPSILON) continue;
		
		// ========== BRDF EVALUATION ==========
		vec3 H = normalize(V + L);
		
		float NdotL = max(dot(N, L), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float LdotH = max(dot(L, H), 0.0);
		float LdotV = max(dot(L, V), 0.0);
		
		if (NdotL < EPSILON) continue;
		
		float softFactor = GetSoftShadow(L, NdotL, lightRadius);
		shadowFactor *= softFactor;
		
		// ========== SPECULAR TERM (Cook-Torrance with energy-conserving Fresnel) ==========
		float D = D_GGX(NdotH, roughness);
		float V_term = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
		vec3 F = F_Schlick(VdotH, F0, f90);
		
		vec3 specular = D * V_term * F;
		
		// ========== DIFFUSE TERM (Burley) ==========
		vec3 kS = F;
		vec3 kD = (1.0 - kS) * (1.0 - metallic);
		
		float burley = Diffuse_Burley(NdotV, NdotL, LdotH, roughness);
		vec3 diffuse = kD * albedo * burley;
		
		// ========== MICRO-OCCLUSION (multi-bounce aware) ==========
		float mbao = GetMultiBounceAO(ao, albedo);
		float microOcclusion = mix(1.0, mbao, 0.5);
		
		// ========== ACCUMULATE ==========
		radiance *= attenuation * shadowFactor;
		Lo += (diffuse + specular) * radiance * NdotL * microOcclusion;
	}
	
	return Lo;
}

// ============ IBL AMBIENT LIGHTING (Enhanced) ============

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
	
	// Multi-bounce diffuse AO
	float diffuseAO = GetMultiBounceAO(ao, albedo);
	vec3 diffuseIBL = irradiance * albedo * kD * diffuseAO;
	
	vec3 R = reflect(-V, N);
	
	// Horizon occlusion: prevent reflections from leaking below the surface plane
	// (Marmoset / Filament technique)
	float horizonFade = min(1.0 + dot(R, N), 1.0);
	horizonFade = horizonFade * horizonFade;
	
	vec3 rotatedR = vec3(
		cosR * R.x - sinR * R.z,
		R.y,
		sinR * R.x + cosR * R.z
	);
	
	float lod = roughness * MAX_REFLECTION_LOD;
	vec3 prefilteredColor = textureLod(u_PrefilteredMap, rotatedR, lod).rgb;
	
	vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
	
	// Multi-scattering energy compensation (Kulla-Conty)
	vec3 energyComp = EnergyCompensation(F0, brdf);
	
	vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y) * energyComp;
	
	// Apply horizon occlusion to specular
	specularIBL *= horizonFade;
	
	// Specular occlusion derived from AO (Lagarde 2014)
	float specAO = ComputeSpecularAO(NdotV, ao, roughness);
	specularIBL *= specAO;
	
	vec3 ambient = (diffuseIBL + specularIBL) * u_IBLIntensity;
	
	return ambient;
}

// ============ FALLBACK AMBIENT (No IBL - Enhanced) ============

vec3 CalculateFallbackAmbient(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
	float NdotV = max(dot(N, V), EPSILON);
	
	vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - metallic);
	
	// Improved hemisphere lighting with warm/cool color ramp
	float skyFactor = N.y * 0.5 + 0.5;
	vec3 skyColor = vec3(0.4, 0.5, 0.65) * 0.6;
	vec3 equatorColor = vec3(0.45, 0.42, 0.38) * 0.35;
	vec3 groundColor = vec3(0.3, 0.25, 0.2) * 0.2;
	
	// Three-color gradient: ground -> equator -> sky
	vec3 hemisphereLight;
	if (skyFactor > 0.5) {
		hemisphereLight = mix(equatorColor, skyColor, (skyFactor - 0.5) * 2.0);
	} else {
		hemisphereLight = mix(groundColor, equatorColor, skyFactor * 2.0);
	}
	
	float diffuseAO = GetMultiBounceAO(ao, albedo);
	vec3 ambientDiffuse = albedo * kD * hemisphereLight * diffuseAO;
	
	vec3 R = reflect(-V, N);
	
	// Horizon occlusion for fallback
	float horizonFade = min(1.0 + dot(R, N), 1.0);
	horizonFade = horizonFade * horizonFade;
	
	float skyRefl = pow(max(R.y, 0.0), 0.5);
	vec3 ambientSpecular = F * mix(groundColor, skyColor, skyRefl) * (1.0 - roughness) * 0.15;
	ambientSpecular *= horizonFade;
	
	// Specular occlusion
	float specAO = ComputeSpecularAO(NdotV, ao, roughness);
	ambientSpecular *= specAO;
	
	return (ambientDiffuse + ambientSpecular) * 1.2;
}

// ============ TONE MAPPING ============

// ACES fitted curve (Stephen Hill) - more accurate than the simple approximation
vec3 ACESFitted(vec3 color) {
	// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
	const mat3 ACESInputMat = mat3(
		0.59719, 0.07600, 0.02840,
		0.35458, 0.90834, 0.13383,
		0.04823, 0.01566, 0.83777
	);
	// ODT_SAT => XYZ => D60_2_D65 => sRGB
	const mat3 ACESOutputMat = mat3(
		 1.60475, -0.10208, -0.00327,
		-0.53108,  1.10813, -0.07276,
		-0.07367, -0.00605,  1.07602
	);
	
	color = ACESInputMat * color;
	
	// RRT and ODT fit
	vec3 a = color * (color + 0.0245786) - 0.000090537;
	vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
	color = a / b;
	
	color = ACESOutputMat * color;
	
	return clamp(color, 0.0, 1.0);
}

// Keep the simple ACES as fallback reference
vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ============ PARALLAX OCCLUSION MAPPING ============

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDirTangent) {
	const int minLayers = 8;
	const int maxLayers = 32;
	float numLayers = mix(float(maxLayers), float(minLayers), abs(viewDirTangent.z));
	
	float layerDepth = 1.0 / numLayers;
	float currentLayerDepth = 0.0;
	vec2 p = viewDirTangent.xy * u_HeightScale;
	vec2 deltaTexCoords = p / numLayers;
	
	vec2 currentTexCoords = texCoords;
	float currentDepthMapValue = texture(u_HeightMap, currentTexCoords).r;
	
	for (int i = 0; i < maxLayers; i++) {
		if (currentLayerDepth >= currentDepthMapValue) break;
		currentTexCoords -= deltaTexCoords;
		currentDepthMapValue = texture(u_HeightMap, currentTexCoords).r;
		currentLayerDepth += layerDepth;
	}
	
	// Occlusion interpolation
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
	float afterDepth = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = texture(u_HeightMap, prevTexCoords).r - currentLayerDepth + layerDepth;
	float weight = afterDepth / (afterDepth - beforeDepth);
	
	return mix(currentTexCoords, prevTexCoords, weight);
}

// ============ COLOR SPACE CONVERSION ============
// Color space constants
#define COLORSPACE_SRGB 0
#define COLORSPACE_LINEAR 1
#define COLORSPACE_LINEAR_REC709 2

// sRGB to Linear conversion (accurate, not just pow 2.2)
vec3 SRGBToLinear(vec3 srgb) {
	return pow(srgb, vec3(2.2));
}

// Apply color space conversion based on the specified color space
// For textures that are already linear (normal maps, ORM, etc.), no conversion needed
// For sRGB textures (albedo, emission), apply gamma-to-linear
// For Linear Rec.709, apply Rec.709 transfer function
vec3 ApplyColorSpace(vec3 texColor, int colorSpace) {
	if (colorSpace == COLORSPACE_SRGB) {
		return pow(texColor, vec3(2.2));
	} else if (colorSpace == COLORSPACE_LINEAR_REC709) {
		// Rec.709 OETF inverse (linearize)
		// threshold at 0.081
		vec3 result;
		result.r = texColor.r < 0.081 ? texColor.r / 4.5 : pow((texColor.r + 0.099) / 1.099, 1.0 / 0.45);
		result.g = texColor.g < 0.081 ? texColor.g / 4.5 : pow((texColor.g + 0.099) / 1.099, 1.0 / 0.45);
		result.b = texColor.b < 0.081 ? texColor.b / 4.5 : pow((texColor.b + 0.099) / 1.099, 1.0 / 0.45);
		return result;
	}
	// COLORSPACE_LINEAR - already linear, no conversion
	return texColor;
}

// ============ EXTRACT CHANNEL FROM LAYERED (ORM) TEXTURE ============

float ExtractChannel(vec3 ormSample, int channel) {
	if (channel == 0) return ormSample.r;
	if (channel == 1) return ormSample.g;
	return ormSample.b;
}

// ============ MAIN FRAGMENT SHADER ============

void main() {
	// ========== UV CALCULATION ==========
	vec2 texCoords = Input.TexCoords * u_UVTiling + u_UVOffset;
	
	// ========== PARALLAX MAPPING ==========
	if (u_UseHeightMap != 0) {
		vec3 viewDirWorld = normalize(u_ViewPos - Input.FragPos);
		vec3 viewDirTangent = normalize(transpose(Input.TBN) * viewDirWorld);
		texCoords = ParallaxMapping(texCoords, viewDirTangent);
		
		// Discard fragments outside [0,1] UV range for parallax
		if (texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
			discard;
	}
	
	// ========== TEXTURE SAMPLING ==========
	
	vec3 albedo;
	float alpha = u_Color.a;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, texCoords);
		albedo = ApplyColorSpace(texColor.rgb, u_AlbedoColorSpace);
		alpha *= texColor.a;
	} else {
		albedo = pow(u_Color.rgb, vec3(2.2));
	}
	
	// ========== ALPHA CUTOFF ==========
	if (u_AlphaMode == 1 && alpha < u_AlphaCutoff) {
		discard;
	}
	
	// ========== NORMAL MAPPING ==========
	vec3 N;
	if (u_UseNormalMap != 0) {
		vec3 normalSample = texture(u_NormalMap, texCoords).rgb;
		// Apply color space for normal map data
		if (u_NormalColorSpace == COLORSPACE_LINEAR_REC709) {
			normalSample = ApplyColorSpace(normalSample, COLORSPACE_LINEAR_REC709);
		}
		vec3 normalMap = normalSample * 2.0 - 1.0;
		// Flip green channel (Y) for DirectX-style normal maps
		if (u_FlipNormalMapY != 0) {
			normalMap.y = -normalMap.y;
		}
		normalMap.xy *= u_NormalIntensity;
		N = normalize(Input.TBN * normalMap);
	} else {
		N = normalize(Input.Normal);
	}
	
	// ========== DETAIL NORMAL BLENDING ==========
	if (u_UseDetailNormalMap != 0) {
		vec2 detailUV = Input.TexCoords * u_DetailUVTiling;
		vec3 detailSample = texture(u_DetailNormalMap, detailUV).rgb;
		vec3 detailNormal = detailSample * 2.0 - 1.0;
		// Apply same Y-flip to detail normal if enabled
		if (u_FlipNormalMapY != 0) {
			detailNormal.y = -detailNormal.y;
		}
		detailNormal.xy *= u_DetailNormalScale;
		
		// UDN blending (Unreal-style): combine normals in tangent space
		vec3 baseNormalTS = transpose(Input.TBN) * N;
		vec3 blended = vec3(baseNormalTS.xy + detailNormal.xy, baseNormalTS.z);
		N = normalize(Input.TBN * blended);
	}
	
	// ========== PBR PARAMETER SAMPLING ==========
	float metallic = u_Metallic;
	float roughness = u_Roughness;
	float ao = 1.0;
	
	if (u_UseLayeredMap != 0) {
		// Sample from packed ORM texture
		vec3 ormSample = texture(u_LayeredMap, texCoords).rgb;
		// Apply color space conversion for layered texture
		ormSample = ApplyColorSpace(ormSample, u_LayeredColorSpace);
		metallic = clamp(ExtractChannel(ormSample, u_LayeredChannelMetallic) * u_MetallicMultiplier, 0.0, 1.0);
		roughness = clamp(ExtractChannel(ormSample, u_LayeredChannelRoughness) * u_RoughnessMultiplier, 0.0, 1.0);
		ao = clamp(ExtractChannel(ormSample, u_LayeredChannelAO) * u_AOMultiplier, 0.0, 1.0);
	} else {
		if (u_UseMetallicMap != 0) {
			metallic = clamp(texture(u_MetallicMap, texCoords).r * u_MetallicMultiplier, 0.0, 1.0);
		}
		if (u_UseRoughnessMap != 0) {
			roughness = clamp(texture(u_RoughnessMap, texCoords).r * u_RoughnessMultiplier, 0.0, 1.0);
		}
		if (u_UseAOMap != 0) {
			ao = clamp(texture(u_AOMap, texCoords).r * u_AOMultiplier, 0.0, 1.0);
		}
	}
	
	roughness = max(roughness, MIN_ROUGHNESS);
	
	// Specular anti-aliasing: only apply when normal maps are active (high-frequency normals)
	if (u_UseNormalMap != 0 || u_UseDetailNormalMap != 0) {
		roughness = SpecularAntiAliasing(N, roughness);
	}
	
	float specular = u_Specular;
	if (u_UseSpecularMap != 0) {
		specular = clamp(texture(u_SpecularMap, texCoords).r * u_SpecularMultiplier, 0.0, 1.0);
	}
	
	// ========== PBR LIGHTING ==========
	
	vec3 V = normalize(u_ViewPos - Input.FragPos);
	
	vec3 F0 = vec3(MIN_DIELECTRIC_F0 * specular);
	F0 = mix(F0, albedo, metallic);

	vec3 directLighting = CalculateDirectLighting(N, V, F0, albedo, metallic, roughness, Input.FragPos, ao);
	
	vec3 ambient;
	if (u_UseIBL != 0) {
		ambient = CalculateIBLAmbient(N, V, F0, albedo, metallic, roughness, ao);
	} else {
		ambient = CalculateFallbackAmbient(N, V, F0, albedo, metallic, roughness, ao);
	}
	
	// ========== EMISSION ==========
	
	vec3 emission = vec3(0.0);
	float emissiveLuminance = 0.0;
	
	if (u_EmissionIntensity > EPSILON) {
		emission = u_EmissionColor * u_EmissionIntensity;
		if (u_UseEmissionMap != 0) {
			vec3 emissionTex = texture(u_EmissionMap, texCoords).rgb;
			emissionTex = ApplyColorSpace(emissionTex, u_EmissionColorSpace);
			emission = emissionTex * u_EmissionColor * u_EmissionIntensity;
		}
		
		emissiveLuminance = dot(emission, vec3(0.2126, 0.7152, 0.0722));
	}
	
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
		float ambientShadow = mix(0.3, 1.0, g_DirectionalShadowFactor);
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
	
	// Tone mapping - use the more accurate ACES fitted curve
	color = ACESFitted(color);
	
	// Gamma correction
	color = pow(color, vec3(1.0 / 2.2));
	
	FragColor = vec4(color, alpha);
	o_EntityID = v_EntityID;
}

#endif