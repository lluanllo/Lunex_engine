#version 450 core

/**
 * Deferred Rendering - Lighting Pass
 * 
 * Full-screen quad that reads G-Buffer textures and computes PBR lighting.
 * Produces the final HDR image with tone mapping and gamma correction.
 * 
 * G-Buffer inputs:
 *   gAlbedoMetallic   (sampler2D, slot 0):  Albedo.rgb + Metallic
 *   gNormalRoughness  (sampler2D, slot 1):  Normal.xyz + Roughness
 *   gEmissionAO       (sampler2D, slot 2):  Emission.rgb + AO
 *   gPositionSpecular (sampler2D, slot 3):  Position.xyz + Specular
 *   gDepth            (sampler2D, slot 4):  Depth buffer
 */

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoords;

layout(location = 0) out vec2 v_TexCoords;

void main() {
	v_TexCoords = a_TexCoords;
	gl_Position = vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 v_TexCoords;

// ============ G-BUFFER SAMPLERS ============
layout (binding = 0) uniform sampler2D gAlbedoMetallic;
layout (binding = 1) uniform sampler2D gNormalRoughness;
layout (binding = 2) uniform sampler2D gEmissionAO;
layout (binding = 3) uniform sampler2D gPositionSpecular;
layout (binding = 4) uniform sampler2D gDepth;

// ============ CAMERA UBO ============
layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
	mat4 u_View;
	mat4 u_Projection;
	vec3 u_ViewPos;
	float _cameraPad;
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
	float _padding3;
	float _padding4;
	float _padding5;
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
	mat4 ViewProjection;
	float SplitDepth;
	float _pad1;
	float _pad2;
	float _pad3;
};

struct ShadowLightData {
	mat4 ViewProjection[6];
	vec4 ShadowParams;
	vec4 ShadowParams2;
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

// ============ IBL SAMPLERS ============
layout (binding = 8) uniform samplerCube u_IrradianceMap;
layout (binding = 9) uniform samplerCube u_PrefilteredMap;
layout (binding = 10) uniform sampler2D u_BRDFLUT;

// ============ SHADOW ATLAS SAMPLER ============
layout (binding = 11) uniform sampler2DArrayShadow u_ShadowAtlas;

// ============ IBL UNIFORM ============
layout(std140, binding = 5) uniform IBLData {
	float u_IBLIntensity;
	float u_IBLRotation;
	int u_UseIBL;
	float _iblPadding;
};

// ============ POST-PROCESSING ============
uniform int u_SkipToneMapGamma;  // 1 = output linear HDR (post-process will handle it)

// ============ CONSTANTS ============
const float PI = 3.14159265359;
const float EPSILON = 0.00001;
const float MIN_DIELECTRIC_F0 = 0.04;
const float MAX_REFLECTION_LOD = 4.0;
const float LIGHT_FALLOFF_BIAS = 0.0001;

// ============ PBR FUNCTIONS ============

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

float Diffuse_Burley(float NdotV, float NdotL, float LdotH, float roughness) {
	float f90 = 0.5 + 2.0 * LdotH * LdotH * roughness;
	float lightScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotL, 5.0);
	float viewScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotV, 5.0);
	return lightScatter * viewScatter / PI;
}

// ============ ATTENUATION ============

float GetPhysicalAttenuation(float distance, float range) {
	if (range <= 0.0) return 1.0;
	float attenuation = 1.0 / max(distance * distance + LIGHT_FALLOFF_BIAS, EPSILON);
	float distRatio = min(distance / range, 1.0);
	float window = pow(max(1.0 - pow(distRatio, 4.0), 0.0), 2.0);
	return attenuation * window;
}

float GetSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
	float cosAngle = dot(-L, spotDir);
	float epsilon_val = max(innerCone - outerCone, EPSILON);
	float intensity = clamp((cosAngle - outerCone) / epsilon_val, 0.0, 1.0);
	return intensity * intensity * (3.0 - 2.0 * intensity);
}

vec3 GetLightIntensity(vec3 color, float intensity, float distance, float radius) {
	float energyScale = 1.0 / max(radius * radius, 1.0);
	return color * intensity * energyScale;
}

float GetSoftShadow(vec3 L, float NdotL, float lightRadius) {
	float softness = lightRadius * (1.0 - NdotL);
	return smoothstep(-softness, softness, NdotL);
}

// ============ SHADOW SAMPLING ============

const vec2 poissonDisk[16] = vec2[](
	vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
	vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
	vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
	vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
	vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
	vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
	vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
	vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
);

float SampleShadowPCF(float layer, vec2 uv, float compareDepth, float radius, float resolution) {
	float texelSize = 1.0 / resolution;
	float shadow = 0.0;
	for (int i = 0; i < 16; i++) {
		vec2 offset = poissonDisk[i] * radius * texelSize;
		shadow += texture(u_ShadowAtlas, vec4(uv + offset, layer, compareDepth));
	}
	return shadow / 16.0;
}

float CalculateDirectionalShadow(int shadowIndex, vec3 fragPos, vec3 normal, float viewDepth) {
	if (shadowIndex < 0 || shadowIndex >= u_NumShadowLights) return 1.0;
	
	ShadowLightData sd = u_Shadows[shadowIndex];
	float firstLayer = sd.ShadowParams.z;
	float bias = sd.ShadowParams.x;
	float normalBias = sd.ShadowParams.y;
	float pcfRadius = sd.ShadowParams2.y;
	float resolution = sd.ShadowParams2.w;
	
	int cascadeIndex = u_CSMCascadeCount - 1;
	for (int c = 0; c < u_CSMCascadeCount; c++) {
		if (viewDepth < u_Cascades[c].SplitDepth) {
			cascadeIndex = c;
			break;
		}
	}
	
	float cascadeBiasScale = 1.0 + float(cascadeIndex) * 0.5;
	float adjustedBias = bias * cascadeBiasScale;
	float adjustedNormalBias = normalBias * cascadeBiasScale;
	
	vec3 biasedPos = fragPos + normal * adjustedNormalBias;
	vec4 lightSpacePos = u_Cascades[cascadeIndex].ViewProjection * vec4(biasedPos, 1.0);
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5;
	
	if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
		projCoords.y < 0.0 || projCoords.y > 1.0 ||
		projCoords.z > 1.0 || projCoords.z < 0.0) {
		return 1.0;
	}
	
	float compareDepth = clamp(projCoords.z - adjustedBias, 0.0, 1.0);
	float layer = firstLayer + float(cascadeIndex);
	float cascadePCFRadius = pcfRadius * (1.0 + float(cascadeIndex) * 0.3);
	float shadow = SampleShadowPCF(layer, projCoords.xy, compareDepth, cascadePCFRadius, resolution);
	
	float fadeStart = u_MaxShadowDistance * 0.8;
	float fadeFactor = clamp((u_MaxShadowDistance - viewDepth) / (u_MaxShadowDistance - fadeStart + 0.001), 0.0, 1.0);
	shadow = mix(1.0, shadow, fadeFactor);
	
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
	return SampleShadowPCF(layer, projCoords.xy, compareDepth, pcfRadius, resolution);
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
	float currentDist = length(fragToLight);
	if (currentDist > lightRange) return 1.0;
	
	float normalBiasScale = max(normalBias, normalBias * (currentDist / lightRange));
	vec3 biasedFrag = fragPos + normal * normalBiasScale;
	float biasedDist = length(biasedFrag - lightPos);
	
	// Determine which cubemap face to sample
	vec3 absDir = abs(fragToLight);
	int face;
	if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
		face = fragToLight.x > 0.0 ? 0 : 1;
	} else if (absDir.y >= absDir.x && absDir.y >= absDir.z) {
		face = fragToLight.y > 0.0 ? 2 : 3;
	} else {
		face = fragToLight.z > 0.0 ? 4 : 5;
	}
	
	// Project onto the selected face
	vec4 lightSpacePos = sd.ViewProjection[face] * vec4(biasedFrag, 1.0);
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5;
	
	// UV boundary check with small margin to avoid face-edge artifacts
	float margin = 2.0 / max(resolution, 1.0);
	if (projCoords.x < margin || projCoords.x > 1.0 - margin ||
		projCoords.y < margin || projCoords.y > 1.0 - margin) return 1.0;
	
	// Compare using linear depth (matches gl_FragDepth = dist/range in depth shader)
	float normalizedDist = biasedDist / lightRange;
	float adaptiveBias = bias * mix(0.2, 1.5, currentDist / lightRange);
	float compareDepth = clamp(normalizedDist - adaptiveBias, 0.0, 1.0);
	
	float layer = firstLayer + float(face);
	float pointPCFRadius = max(0.5, 1.0 * (1.0 - currentDist / lightRange));
	return SampleShadowPCF(layer, projCoords.xy, compareDepth, pointPCFRadius, resolution);
}

// ============ DIRECT LIGHTING ============

float g_DirectionalShadowFactor = 1.0;

vec3 CalculateDirectLighting(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, vec3 fragPos, float ao, float viewDepth) {
	vec3 Lo = vec3(0.0);
	float NdotV = max(dot(N, V), EPSILON);
	
	int lightCount = min(u_NumLights, MAX_LIGHTS);
	g_DirectionalShadowFactor = 1.0;
	
	int dirShadowIndices[MAX_SHADOW_LIGHTS];
	int spotShadowIndices[MAX_SHADOW_LIGHTS];
	int pointShadowIndices[MAX_SHADOW_LIGHTS];
	int numDirShadows = 0, numSpotShadows = 0, numPointShadows = 0;

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
		float shadowFactor = 1.0;
		bool castsShadows = light.Params.z > 0.5;
		
		if (lightType == 0) {
			L = normalize(-light.Direction.xyz);
			radiance = light.Color.rgb * light.Color.a;
			lightRadius = 0.0;
			if (castsShadows && dirCounter < numDirShadows) {
				shadowFactor = CalculateDirectionalShadow(dirShadowIndices[dirCounter], fragPos, N, viewDepth);
				dirCounter++;
				g_DirectionalShadowFactor = min(g_DirectionalShadowFactor, shadowFactor);
			}
		} else if (lightType == 1) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / max(distance, EPSILON);
			float range = light.Direction.w;
			if (range > 0.0 && distance > range) { if (castsShadows) pointCounter++; continue; }
			attenuation = GetPhysicalAttenuation(distance, range);
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.05, 0.05);
			if (castsShadows && pointCounter < numPointShadows) {
				shadowFactor = CalculatePointShadow(pointShadowIndices[pointCounter], fragPos, N, light.Position.xyz);
				pointCounter++;
			}
		} else if (lightType == 2) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / max(distance, EPSILON);
			float range = light.Direction.w;
			if (range > 0.0 && distance > range) { if (castsShadows) spotCounter++; continue; }
			attenuation = GetPhysicalAttenuation(distance, range);
			vec3 spotDir = normalize(light.Direction.xyz);
			float spotAtten = GetSpotAttenuation(L, spotDir, light.Params.x, light.Params.y);
			attenuation *= spotAtten;
			if (attenuation < EPSILON) { if (castsShadows) spotCounter++; continue; }
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.08, 0.08);
			if (castsShadows && spotCounter < numSpotShadows) {
				shadowFactor = CalculateSpotShadow(spotShadowIndices[spotCounter], fragPos, N);
				spotCounter++;
			}
		}
		
		if (attenuation < EPSILON) continue;
		
		vec3 H = normalize(V + L);
		float NdotL = max(dot(N, L), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float LdotH = max(dot(L, H), 0.0);
		
		if (NdotL < EPSILON) continue;
		
		float softFactor = GetSoftShadow(L, NdotL, lightRadius);
		shadowFactor *= softFactor;
		
		float D = D_GGX(NdotH, roughness);
		float V_term = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
		vec3 F = F_Schlick(VdotH, F0);
		vec3 spec = D * V_term * F;
		
		vec3 kS = F;
		vec3 kD = (1.0 - kS) * (1.0 - metallic);
		float burley = Diffuse_Burley(NdotV, NdotL, LdotH, roughness);
		vec3 diffuse = kD * albedo * burley;
		
		float microOcclusion = mix(1.0, ao, 0.5);
		radiance *= attenuation * shadowFactor;
		Lo += (diffuse + spec) * radiance * NdotL * microOcclusion;
	}
	
	return Lo;
}

// ============ IBL AMBIENT ============

vec3 CalculateIBLAmbient(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
	float NdotV = max(dot(N, V), EPSILON);
	vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - metallic);
	
	float cosR = cos(u_IBLRotation);
	float sinR = sin(u_IBLRotation);
	vec3 rotatedN = vec3(cosR * N.x - sinR * N.z, N.y, sinR * N.x + cosR * N.z);
	
	vec3 irradiance = texture(u_IrradianceMap, rotatedN).rgb;
	vec3 diffuseIBL = irradiance * albedo * kD;
	
	vec3 R = reflect(-V, N);
	vec3 rotatedR = vec3(cosR * R.x - sinR * R.z, R.y, sinR * R.x + cosR * R.z);
	
	float lod = roughness * MAX_REFLECTION_LOD;
	vec3 prefilteredColor = textureLod(u_PrefilteredMap, rotatedR, lod).rgb;
	vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
	vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
	
	return (diffuseIBL + specularIBL) * u_IBLIntensity * ao;
}

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

// ============ TONE MAPPING ============

vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ============ MAIN ============

void main() {
	// Sample G-Buffer
	vec4 albedoMetallic  = texture(gAlbedoMetallic,   v_TexCoords);
	vec4 normalRoughness = texture(gNormalRoughness,   v_TexCoords);
	vec4 emissionAO      = texture(gEmissionAO,        v_TexCoords);
	vec4 positionSpecular = texture(gPositionSpecular, v_TexCoords);
	float depthSample    = texture(gDepth,             v_TexCoords).r;
	
	// Early out for sky pixels (nothing was written to G-Buffer)
	if (depthSample >= 1.0) {
		discard;
	}
	
	// Unpack G-Buffer
	vec3 albedo   = albedoMetallic.rgb;
	float metallic = albedoMetallic.a;
	
	vec3 N        = normalize(normalRoughness.rgb * 2.0 - 1.0);  // Decode from [0,1] back to [-1,1]
	float roughness = normalRoughness.a;
	
	vec3 emission = emissionAO.rgb;
	float ao      = emissionAO.a;
	
	vec3 fragPos  = positionSpecular.rgb;
	float specular = positionSpecular.a;
	
	// Compute view depth (for CSM cascade selection)
	vec4 clipPos = u_ViewProjection * vec4(fragPos, 1.0);
	float viewDepth = clipPos.w;
	
	// PBR Lighting
	vec3 V = normalize(u_ViewPos - fragPos);
	
	vec3 F0 = vec3(MIN_DIELECTRIC_F0 * specular * 2.0);
	F0 = mix(F0, albedo, metallic);
	
	vec3 directLighting = CalculateDirectLighting(N, V, F0, albedo, metallic, roughness, fragPos, ao, viewDepth);
	
	vec3 ambient;
	if (u_UseIBL != 0) {
		ambient = CalculateIBLAmbient(N, V, F0, albedo, metallic, roughness, ao);
	} else {
		ambient = CalculateFallbackAmbient(N, V, F0, albedo, metallic, roughness, ao);
	}
	
	// Emission contribution
	vec3 emissiveContribution = vec3(0.0);
	float emissiveLuminance = dot(emission, vec3(0.2126, 0.7152, 0.0722));
	if (emissiveLuminance > EPSILON) {
		vec3 emissiveLight = emission * 0.3;
		float upwardBias = max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0) * 0.5 + 0.5;
		float kD = (1.0 - metallic);
		emissiveContribution = albedo * emissiveLight * kD * upwardBias * ao;
	}
	
	// Shadow modulation on ambient
	if (u_NumShadowLights > 0 && g_DirectionalShadowFactor < 1.0) {
		float ambientShadow = mix(0.35, 1.0, g_DirectionalShadowFactor);
		ambient *= ambientShadow;
	}
	
	vec3 color = ambient + directLighting + emission + emissiveContribution;
	
	// Sky tinting
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
		color += skyColor * u_SkyTintStrength * shadowAmount * albedo;
	}
	
	// Tone mapping + gamma (skip if post-processing will handle it)
	if (u_SkipToneMapGamma == 0) {
		color = ACESFilm(color);
		color = pow(color, vec3(1.0 / 2.2));
	}
	
	FragColor = vec4(color, 1.0);
}

#endif
