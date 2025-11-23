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

void main() {
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	Output.FragPos = worldPos.xyz;
	
	// Optimized normal matrix calculation
	mat3 normalMatrix = mat3(transpose(inverse(u_Transform)));
	vec3 N = normalize(normalMatrix * a_Normal);
	Output.Normal = N;
	Output.TexCoords = a_TexCoords;
	
	// Enhanced TBN with proper handedness handling
	vec3 T = normalize(normalMatrix * a_Tangent);
	T = normalize(T - dot(T, N) * N);
	vec3 B = normalize(normalMatrix * a_Bitangent);
	// Ensure orthogonality and handedness
	B = normalize(B - dot(B, N) * N - dot(B, T) * T);
	Output.TBN = mat3(T, B, N);
	
	v_EntityID = a_EntityID;
	gl_Position = u_ViewProjection * worldPos;
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

// ============ LIGHTING SYSTEM ============
#define MAX_LIGHTS 16

struct LightData {
	vec4 Position;
	vec4 Direction;
	vec4 Color;
	vec4 Params;
	vec4 Attenuation;
};

layout(std140, binding = 3) uniform Lights {
	int u_NumLights;
	float _padding3;
	float _padding4;
	float _padding5;
	LightData u_Lights[MAX_LIGHTS];
};

// ============ TEXTURE SAMPLERS ============
layout (binding = 0) uniform sampler2D u_AlbedoMap;
layout (binding = 1) uniform sampler2D u_NormalMap;
layout (binding = 2) uniform sampler2D u_MetallicMap;
layout (binding = 3) uniform sampler2D u_RoughnessMap;
layout (binding = 4) uniform sampler2D u_SpecularMap;
layout (binding = 5) uniform sampler2D u_EmissionMap;
layout (binding = 6) uniform sampler2D u_AOMap;

// ============ CONSTANTS ============
const float PI = 3.14159265359;
const float EPSILON = 0.00001;
const float MIN_ROUGHNESS = 0.045;
const float MIN_DIELECTRIC_F0 = 0.04;

// ============ ENHANCED PBR FUNCTIONS ============

// GGX/Trowbridge-Reitz Normal Distribution Function
float D_GGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	
	float denom = NdotH2 * (a2 - 1.0) + 1.0;
	denom = PI * denom * denom;
	
	return a2 / max(denom, EPSILON);
}

// Smith's Joint Approximation with Height-Correlated G2
float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness) {
	float a2 = roughness * roughness;
	
	float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	
	return 0.5 / max(GGXV + GGXL, EPSILON);
}

// Schlick-GGX with improved edge cases
float G_SchlickGGX(float NdotV, float roughness) {
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	
	return NdotV / max(NdotV * (1.0 - k) + k, EPSILON);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
	return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick with spherical gaussian approximation
vec3 F_Schlick(float VdotH, vec3 F0) {
	float f = pow(1.0 - VdotH, 5.0);
	return F0 + (1.0 - F0) * f;
}

// Enhanced Fresnel with roughness (for IBL)
vec3 F_SchlickRoughness(float NdotV, vec3 F0, float roughness) {
	float smoothness = 1.0 - roughness;
	return F0 + (max(vec3(smoothness), F0) - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
}

// Energy-conserving wrapped diffuse for better subsurface appearance
vec3 DiffuseLambert(vec3 albedo) {
	return albedo / PI;
}

// Burley/Disney Diffuse BRDF (optional, more accurate but expensive)
float Fd_Burley(float NdotV, float NdotL, float LdotH, float roughness) {
	float f90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
	float lightScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotL, 5.0);
	float viewScatter = 1.0 + (f90 - 1.0) * pow(1.0 - NdotV, 5.0);
	return lightScatter * viewScatter / PI;
}

// ============ ADVANCED LIGHTING FEATURES ============

// Distance-based attenuation with smooth windowing function
float GetDistanceAttenuation(float distance, float range) {
	if (range <= 0.0) return 1.0;
	
	// Smooth window function (better than hard cutoff)
	float distRatio = distance / range;
	float window = pow(max(1.0 - pow(distRatio, 4.0), 0.0), 2.0);
	
	// Physical inverse square falloff
	float attenuation = 1.0 / max(distance * distance, 0.0001);
	
	return attenuation * window;
}

// Enhanced spot light cone with smoother falloff
float GetSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
	float cosAngle = dot(-L, spotDir);
	float epsilon = innerCone - outerCone;
	float spotFactor = clamp((cosAngle - outerCone) / max(epsilon, EPSILON), 0.0, 1.0);
	
	// Smoothstep for better visual quality
	return spotFactor * spotFactor * (3.0 - 2.0 * spotFactor);
}

// ============ MAIN LIGHTING CALCULATION ============

vec3 CalculateDirectLighting(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, vec3 fragPos) {
	vec3 Lo = vec3(0.0);
	float NdotV = max(dot(N, V), 0.0);
	
	for (int i = 0; i < min(u_NumLights, MAX_LIGHTS); i++) {
		LightData light = u_Lights[i];
		int lightType = int(light.Position.w);
		
		vec3 L;
		vec3 radiance = light.Color.rgb * light.Color.a;
		float attenuation = 1.0;
		
		// ========== LIGHT TYPE PROCESSING ==========
		if (lightType == 0) {
			// Directional Light
			L = normalize(-light.Direction.xyz);
		}
		else if (lightType == 1) {
			// Point Light
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / distance;
			
			float range = light.Params.x;
			if (range > 0.0 && distance > range) continue;
			
			attenuation = GetDistanceAttenuation(distance, range);
		}
		else if (lightType == 2) {
			// Spot Light
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / distance;
			
			float range = light.Params.x;
			if (range > 0.0 && distance > range) continue;
			
			attenuation = GetDistanceAttenuation(distance, range);
			
			vec3 spotDir = normalize(light.Direction.xyz);
			float spotAtten = GetSpotAttenuation(L, spotDir, light.Params.y, light.Params.z);
			attenuation *= spotAtten;
		}
		
		if (attenuation < EPSILON) continue;
		
		// ========== BRDF EVALUATION ==========
		vec3 H = normalize(V + L);
		
		float NdotL = max(dot(N, L), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float LdotH = max(dot(L, H), 0.0);
		
		// Specular BRDF (Cook-Torrance)
		float D = D_GGX(NdotH, roughness);
		float V_term = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
		vec3 F = F_Schlick(VdotH, F0);
		
		vec3 specular = D * V_term * F;
		
		// Diffuse BRDF (Energy-conserving Lambert or Burley)
		vec3 kS = F;
		vec3 kD = (1.0 - kS) * (1.0 - metallic);
		
		// Use Burley diffuse for better quality (optional: switch to Lambert for performance)
		float diffuseFactor = Fd_Burley(NdotV, NdotL, LdotH, roughness);
		vec3 diffuse = kD * albedo * diffuseFactor;
		
		// Alternative: Simple Lambert
		// vec3 diffuse = kD * DiffuseLambert(albedo);
		
		// Accumulate lighting
		radiance *= attenuation;
		Lo += (diffuse + specular) * radiance * NdotL;
	}
	
	return Lo;
}

// ============ IMPROVED AMBIENT/IBL APPROXIMATION ============

vec3 CalculateAmbient(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
	float NdotV = max(dot(N, V), 0.0);
	
	// Enhanced Fresnel for IBL
	vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - metallic);
	
	// Improved ambient term with better energy conservation
	vec3 ambientDiffuse = albedo * kD;
	
	// Simple specular ambient (in real AAA this would be IBL probes/reflections)
	vec3 ambientSpecular = F * (1.0 - roughness) * 0.02;
	
	vec3 ambient = (ambientDiffuse + ambientSpecular) * ao;
	
	// Scale ambient based on scene (typical AAA value)
	return ambient * 0.03;
}

// ============ TONE MAPPING OPTIONS ============

// ACES Filmic Tone Mapping (industry standard)
vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Uncharted 2 Tone Mapping
vec3 Uncharted2Tonemap(vec3 x) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// ============ MAIN FRAGMENT SHADER ============

void main() {
	// ========== TEXTURE SAMPLING ==========
	
	// Albedo with proper sRGB conversion
	vec3 albedo;
	float alpha = u_Color.a;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, Input.TexCoords);
		albedo = pow(texColor.rgb, vec3(2.2));
		alpha *= texColor.a;
	} else {
		albedo = pow(u_Color.rgb, vec3(2.2));
	}
	
	// Normal mapping with proper unpacking
	vec3 N;
	if (u_UseNormalMap != 0) {
		vec3 normalMap = texture(u_NormalMap, Input.TexCoords).rgb;
		normalMap = normalMap * 2.0 - 1.0;
		N = normalize(Input.TBN * normalMap);
	} else {
		N = normalize(Input.Normal);
	}
	
	// Material properties
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
	
	// ========== PBR LIGHTING ==========
	
	vec3 V = normalize(u_ViewPos - Input.FragPos);
	
	// Calculate F0 (base reflectance at normal incidence)
	vec3 F0 = vec3(MIN_DIELECTRIC_F0);
	F0 = mix(F0, albedo, metallic);
	F0 = mix(F0 * specular, F0, metallic);
	
	// Direct lighting
	vec3 directLighting = CalculateDirectLighting(N, V, F0, albedo, metallic, roughness, Input.FragPos);
	
	// Ambient/IBL approximation
	vec3 ambient = CalculateAmbient(N, V, F0, albedo, metallic, roughness, ao);
	
	// ========== EMISSION ==========
	
	vec3 emission = vec3(0.0);
	float emissiveLuminance = 0.0;
	
	if (u_EmissionIntensity > EPSILON) {
		emission = u_EmissionColor * u_EmissionIntensity;
		if (u_UseEmissionMap != 0) {
			vec3 emissionTex = texture(u_EmissionMap, Input.TexCoords).rgb;
			emissionTex = pow(emissionTex, vec3(2.2));
			emission = emissionTex * u_EmissionColor * u_EmissionIntensity;
		}
		
		// Calculate luminance for emissive light contribution
		emissiveLuminance = dot(emission, vec3(0.2126, 0.7152, 0.0722));
	}
	
	// ========== EMISSIVE SURFACE AS AREA LIGHT ==========
	// Surfaces with emission contribute diffuse lighting to the scene
	vec3 emissiveContribution = vec3(0.0);
	
	if (emissiveLuminance > EPSILON) {
		// Treat emissive surface as a virtual area light
		// This simulates light bouncing from emissive surfaces
		vec3 emissiveLight = emission * 0.25; // Scale factor for indirect contribution
		
		// Apply to diffuse component based on normal orientation
		float upwardBias = max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0) * 0.5 + 0.5;
		
		// Add ambient-like contribution from emission
		float kD = (1.0 - metallic);
		emissiveContribution = albedo * emissiveLight * kD * upwardBias * ao;
	}
	
	// ========== FINAL COMPOSITION ==========
	
	vec3 color = ambient + directLighting + emission + emissiveContribution;
	
	// Tone mapping (ACES for AAA look)
	color = ACESFilm(color);
	
	// Gamma correction
	color = pow(color, vec3(1.0 / 2.2));
	
	FragColor = vec4(color, alpha);
	o_EntityID = v_EntityID;
}

#endif