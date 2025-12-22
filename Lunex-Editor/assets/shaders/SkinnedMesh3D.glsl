#version 450 core

/**
 * SkinnedMesh3D.glsl
 * 
 * Skeletal mesh shader with GPU skinning support.
 * Extends Mesh3D.glsl with bone transformation.
 * 
 * Part of the AAA Animation System
 */

#ifdef VERTEX

// ============ VERTEX ATTRIBUTES ============
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in int a_EntityID;

// ? NEW: Bone weights and indices for skinning
layout(location = 6) in ivec4 a_BoneIDs;      // Up to 4 bones per vertex
layout(location = 7) in vec4 a_BoneWeights;   // Weights for each bone

// ============ UNIFORM BUFFERS ============
layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
};

layout(std140, binding = 1) uniform Transform {
	mat4 u_Transform;
};

// ? NEW: Bone matrices storage buffer (binding 10 to avoid conflicts)
#define MAX_BONES 256

layout(std430, binding = 10) buffer BoneMatrices {
	mat4 u_BoneMatrices[MAX_BONES];
};

// ? NEW: Skinning configuration
layout(std140, binding = 11) uniform SkinningConfig {
	int u_UseSkinning;       // 1 = skinned mesh, 0 = static mesh
	int u_BoneCount;         // Number of bones in skeleton
	float _skinPadding1;
	float _skinPadding2;
};

// ============ VERTEX OUTPUT ============
struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) out VertexOutput Output;
layout (location = 7) out flat int v_EntityID;

// ============ SKINNING FUNCTION ============
mat4 GetBoneTransform() {
	mat4 boneTransform = mat4(0.0);
	
	// Accumulate transforms from up to 4 bones
	for (int i = 0; i < 4; i++) {
		int boneID = a_BoneIDs[i];
		float weight = a_BoneWeights[i];
		
		if (boneID >= 0 && boneID < MAX_BONES && weight > 0.0) {
			boneTransform += u_BoneMatrices[boneID] * weight;
		}
	}
	
	// Handle case where total weight is less than 1 (shouldn't happen but safety)
	float totalWeight = a_BoneWeights.x + a_BoneWeights.y + a_BoneWeights.z + a_BoneWeights.w;
	if (totalWeight < 0.001) {
		return mat4(1.0);  // Identity if no valid bones
	}
	
	return boneTransform;
}

void main() {
	vec4 localPos = vec4(a_Position, 1.0);
	vec3 localNormal = a_Normal;
	vec3 localTangent = a_Tangent;
	vec3 localBitangent = a_Bitangent;
	
	// ? Apply skinning transformation
	if (u_UseSkinning != 0) {
		mat4 boneTransform = GetBoneTransform();
		
		// Transform position
		localPos = boneTransform * localPos;
		
		// Transform normals (using upper 3x3 of bone transform)
		mat3 boneNormalMatrix = mat3(boneTransform);
		localNormal = boneNormalMatrix * localNormal;
		localTangent = boneNormalMatrix * localTangent;
		localBitangent = boneNormalMatrix * localBitangent;
	}
	
	// Apply world transform
	vec4 worldPos = u_Transform * localPos;
	Output.FragPos = worldPos.xyz;
	
	// Normal matrix
	mat3 normalMatrix = mat3(transpose(inverse(u_Transform)));
	
	vec3 N = normalize(normalMatrix * localNormal);
	Output.Normal = N;
	Output.TexCoords = a_TexCoords;
	
	// TBN matrix for normal mapping
	vec3 T = normalize(normalMatrix * localTangent);
	T = normalize(T - dot(T, N) * N);
	vec3 B = normalize(normalMatrix * localBitangent);
	B = normalize(B - dot(B, N) * N - dot(B, T) * T);
	Output.TBN = mat3(T, B, N);
	
	v_EntityID = a_EntityID;
	gl_Position = u_ViewProjection * worldPos;
}

#elif defined(FRAGMENT)

// ============ FRAGMENT OUTPUT ============
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

// ============ MATERIAL PROPERTIES ============
layout(std140, binding = 2) uniform Material {
	vec4 u_Color;
	float u_Metallic;
	float u_Roughness;
	float u_Specular;
	float u_EmissionIntensity;
	vec3 u_EmissionColor;
	float _padding1;
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

// ============ TEXTURE SAMPLERS ============
layout (binding = 0) uniform sampler2D u_AlbedoMap;
layout (binding = 1) uniform sampler2D u_NormalMap;
layout (binding = 2) uniform sampler2D u_MetallicMap;
layout (binding = 3) uniform sampler2D u_RoughnessMap;
layout (binding = 4) uniform sampler2D u_SpecularMap;
layout (binding = 5) uniform sampler2D u_EmissionMap;
layout (binding = 6) uniform sampler2D u_AOMap;

// ============ IBL SAMPLERS ============
layout (binding = 8) uniform samplerCube u_IrradianceMap;
layout (binding = 9) uniform samplerCube u_PrefilteredMap;
layout (binding = 10) uniform sampler2D u_BRDFLUT;

// ============ IBL UNIFORM ============
layout(std140, binding = 5) uniform IBLData {
	float u_IBLIntensity;
	float u_IBLRotation;
	int u_UseIBL;
	float _iblPadding;
};

// ============ CONSTANTS ============
const float PI = 3.14159265359;
const float EPSILON = 0.00001;
const float MIN_ROUGHNESS = 0.045;
const float MIN_DIELECTRIC_F0 = 0.04;
const float MAX_REFLECTION_LOD = 4.0;
const float LIGHT_FALLOFF_BIAS = 0.0001;

// ============ PBR FUNCTIONS ============
// (Same as Mesh3D.glsl - included for completeness)

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
	float a2 = roughness * roughness;
	
	float lambdaV = NdotL * sqrt((-NdotV * a2 + NdotV) * NdotV + a2);
	float lambdaL = NdotV * sqrt((-NdotL * a2 + NdotL) * NdotL + a2);
	
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

float GetPhysicalAttenuation(float distance, float range) {
	if (range <= 0.0) return 1.0;
	
	float attenuation = 1.0 / max(distance * distance + LIGHT_FALLOFF_BIAS, EPSILON);
	float distRatio = min(distance / range, 1.0);
	float window = pow(max(1.0 - pow(distRatio, 4.0), 0.0), 2.0);
	
	return attenuation * window;
}

float GetSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
	float cosAngle = dot(-L, spotDir);
	float epsilon = max(innerCone - outerCone, EPSILON);
	float intensity = clamp((cosAngle - outerCone) / epsilon, 0.0, 1.0);
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

// ============ LIGHTING CALCULATION ============

vec3 CalculateDirectLighting(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, vec3 fragPos, float ao) {
	vec3 Lo = vec3(0.0);
	float NdotV = max(dot(N, V), EPSILON);
	
	int lightCount = min(u_NumLights, MAX_LIGHTS);
	
	for (int i = 0; i < lightCount; i++) {
		LightData light = u_Lights[i];
		int lightType = int(light.Position.w);
		
		vec3 L;
		vec3 radiance;
		float attenuation = 1.0;
		float lightRadius = 0.1;
		
		if (lightType == 0) {
			L = normalize(-light.Direction.xyz);
			radiance = light.Color.rgb * light.Color.a;
			lightRadius = 0.0;
		} else if (lightType == 1) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / max(distance, EPSILON);
			
			float range = light.Direction.w;
			if (range > 0.0 && distance > range) continue;
			
			attenuation = GetPhysicalAttenuation(distance, range);
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.05, 0.05);
		} else if (lightType == 2) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / max(distance, EPSILON);
			
			float range = light.Direction.w;
			if (range > 0.0 && distance > range) continue;
			
			attenuation = GetPhysicalAttenuation(distance, range);
			
			vec3 spotDir = normalize(light.Direction.xyz);
			float spotAtten = GetSpotAttenuation(L, spotDir, light.Params.x, light.Params.y);
			attenuation *= spotAtten;
			
			if (attenuation < EPSILON) continue;
			
			radiance = GetLightIntensity(light.Color.rgb, light.Color.a, distance, max(range * 0.1, 0.1));
			lightRadius = max(range * 0.08, 0.08);
		}
		
		if (attenuation < EPSILON) continue;
		
		vec3 H = normalize(V + L);
		
		float NdotL = max(dot(N, L), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float LdotH = max(dot(L, H), 0.0);
		
		if (NdotL < EPSILON) continue;
		
		float shadowFactor = GetSoftShadow(L, NdotL, lightRadius);
		
		float D = D_GGX(NdotH, roughness);
		float V_term = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
		vec3 F = F_Schlick(VdotH, F0);
		
		vec3 specular = D * V_term * F;
		
		vec3 kS = F;
		vec3 kD = (1.0 - kS) * (1.0 - metallic);
		
		float burley = Diffuse_Burley(NdotV, NdotL, LdotH, roughness);
		vec3 diffuse = kD * albedo * burley;
		
		float microOcclusion = mix(1.0, ao, 0.5);
		
		radiance *= attenuation * shadowFactor;
		Lo += (diffuse + specular) * radiance * NdotL * microOcclusion;
	}
	
	return Lo;
}

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
	
	vec3 ambient = (diffuseIBL + specularIBL) * u_IBLIntensity * ao;
	
	return ambient;
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
	vec3 albedo;
	float alpha = u_Color.a;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, Input.TexCoords);
		albedo = pow(texColor.rgb, vec3(2.2));
		alpha *= texColor.a;
	} else {
		albedo = pow(u_Color.rgb, vec3(2.2));
	}
	
	vec3 N;
	if (u_UseNormalMap != 0) {
		vec3 normalMap = texture(u_NormalMap, Input.TexCoords).rgb;
		normalMap = normalMap * 2.0 - 1.0;
		N = normalize(Input.TBN * normalMap);
	} else {
		N = normalize(Input.Normal);
	}
	
	float metallic = u_Metallic;
	if (u_UseMetallicMap != 0) {
		metallic = clamp(texture(u_MetallicMap, Input.TexCoords).r * u_MetallicMultiplier, 0.0, 1.0);
	}
	
	float roughness = u_Roughness;
	if (u_UseRoughnessMap != 0) {
		roughness = clamp(texture(u_RoughnessMap, Input.TexCoords).r * u_RoughnessMultiplier, 0.0, 1.0);
	}
	roughness = max(roughness, MIN_ROUGHNESS);
	
	float specular = u_Specular;
	if (u_UseSpecularMap != 0) {
		specular = clamp(texture(u_SpecularMap, Input.TexCoords).r * u_SpecularMultiplier, 0.0, 1.0);
	}
	
	float ao = 1.0;
	if (u_UseAOMap != 0) {
		ao = clamp(texture(u_AOMap, Input.TexCoords).r * u_AOMultiplier, 0.0, 1.0);
	}
	
	vec3 V = normalize(u_ViewPos - Input.FragPos);
	
	vec3 F0 = vec3(MIN_DIELECTRIC_F0);
	F0 = mix(F0, albedo, metallic);
	F0 = mix(F0 * specular, F0, metallic);
	
	vec3 directLighting = CalculateDirectLighting(N, V, F0, albedo, metallic, roughness, Input.FragPos, ao);
	
	vec3 ambient;
	if (u_UseIBL != 0) {
		ambient = CalculateIBLAmbient(N, V, F0, albedo, metallic, roughness, ao);
	} else {
		ambient = CalculateFallbackAmbient(N, V, F0, albedo, metallic, roughness, ao);
	}
	
	vec3 emission = vec3(0.0);
	if (u_EmissionIntensity > EPSILON) {
		emission = u_EmissionColor * u_EmissionIntensity;
		if (u_UseEmissionMap != 0) {
			vec3 emissionTex = texture(u_EmissionMap, Input.TexCoords).rgb;
			emissionTex = pow(emissionTex, vec3(2.2));
			emission = emissionTex * u_EmissionColor * u_EmissionIntensity;
		}
	}
	
	vec3 color = ambient + directLighting + emission;
	color = ACESFilm(color);
	color = pow(color, vec3(1.0 / 2.2));
	
	FragColor = vec4(color, alpha);
	o_EntityID = v_EntityID;
}

#endif
