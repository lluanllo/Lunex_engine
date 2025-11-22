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
layout (location = 6) out flat int v_EntityID;

void main() {
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	Output.FragPos = worldPos.xyz;
	
	// Calculate normal matrix (handles non-uniform scaling correctly)
	mat3 normalMatrix = mat3(transpose(inverse(u_Transform)));
	vec3 N = normalize(normalMatrix * a_Normal);
	Output.Normal = N;
	Output.TexCoords = a_TexCoords;
	
	// Calculate TBN matrix for normal mapping (Gram-Schmidt orthogonalization)
	vec3 T = normalize(normalMatrix * a_Tangent);
	T = normalize(T - dot(T, N) * N); // Re-orthogonalize
	vec3 B = cross(N, T);
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
layout (location = 6) in flat int v_EntityID;

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
	
	// Texture flags
	int u_UseAlbedoMap;
	int u_UseNormalMap;
	int u_UseMetallicMap;
	int u_UseRoughnessMap;
	int u_UseSpecularMap;
	int u_UseEmissionMap;
	int u_UseAOMap;
	float _padding2;
	
	// Texture multipliers
	float u_MetallicMultiplier;
	float u_RoughnessMultiplier;
	float u_SpecularMultiplier;
	float u_AOMultiplier;
};

// ============ LIGHTING SYSTEM ============
#define MAX_LIGHTS 16

struct LightData {
	vec4 Position;      // w = light type (0=directional, 1=point, 2=spot)
	vec4 Direction;     // w = unused
	vec4 Color;         // rgb = color, a = intensity
	vec4 Params;        // x = range, y = innerCone, z = outerCone, w = unused
	vec4 Attenuation;   // x = constant, y = linear, z = quadratic, w = unused
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

// ============ PBR FUNCTIONS ============

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;
	
	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return num / max(denom, EPSILON);
}

// Geometry Function (Smith's Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	
	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;
	
	return num / max(denom, EPSILON);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
	return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness for IBL
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============ LIGHTING CALCULATION ============

vec3 CalculatePBRLighting(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, vec3 fragPos) {
	vec3 Lo = vec3(0.0);
	
	for (int i = 0; i < u_NumLights && i < MAX_LIGHTS; i++) {
		LightData light = u_Lights[i];
		int lightType = int(light.Position.w);
		
		vec3 L;
		float attenuation = 1.0;
		vec3 radiance = light.Color.rgb * light.Color.a;
		
		// ========== LIGHT TYPE CALCULATIONS ==========
		if (lightType == 0) {
			// Directional Light
			L = normalize(-light.Direction.xyz);
		}
		else if (lightType == 1) {
			// Point Light
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = normalize(lightVec);
			
			// Range check
			if (distance > light.Params.x && light.Params.x > 0.0) {
				continue;
			}
			
			// Physically-based attenuation (inverse square law with smoothing)
			float constant = light.Attenuation.x;
			float linear = light.Attenuation.y;
			float quadratic = light.Attenuation.z;
			attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
		}
		else if (lightType == 2) {
			// Spot Light
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = normalize(lightVec);
			
			// Range check
			if (distance > light.Params.x && light.Params.x > 0.0) {
				continue;
			}
			
			// Attenuation
			float constant = light.Attenuation.x;
			float linear = light.Attenuation.y;
			float quadratic = light.Attenuation.z;
			attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
			
			// Spot cone smooth falloff
			float theta = dot(L, normalize(-light.Direction.xyz));
			float innerCone = light.Params.y;
			float outerCone = light.Params.z;
			float epsilon = innerCone - outerCone;
			float intensity = clamp((theta - outerCone) / max(epsilon, EPSILON), 0.0, 1.0);
			// Smooth step for better falloff
			intensity = intensity * intensity * (3.0 - 2.0 * intensity);
			attenuation *= intensity;
		}
		
		if (attenuation <= EPSILON) continue;
		
		// ========== COOK-TORRANCE BRDF ==========
		vec3 H = normalize(V + L);
		float NdotL = max(dot(N, L), 0.0);
		
		// Apply attenuation to radiance
		radiance *= attenuation;
		
		// Calculate Cook-Torrance BRDF components
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
		
		vec3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL;
		vec3 specular = numerator / max(denominator, EPSILON);
		
		// Energy conservation
		vec3 kS = F; // Specular contribution
		vec3 kD = vec3(1.0) - kS; // Diffuse contribution
		kD *= 1.0 - metallic; // Metals don't have diffuse
		
		// Final lighting contribution
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}
	
	return Lo;
}

// ============ MAIN FRAGMENT SHADER ============

void main() {
	// ========== SAMPLE TEXTURES ==========
	
	// Albedo (with gamma correction)
	vec3 albedo;
	float alpha = u_Color.a;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, Input.TexCoords);
		albedo = pow(texColor.rgb, vec3(2.2)); // sRGB to linear space
		alpha *= texColor.a;
	} else {
		albedo = pow(u_Color.rgb, vec3(2.2)); // sRGB to linear space
	}
	
	// Normal mapping
	vec3 N;
	if (u_UseNormalMap != 0) {
		vec3 normalMapSample = texture(u_NormalMap, Input.TexCoords).rgb;
		normalMapSample = normalMapSample * 2.0 - 1.0; // [0,1] to [-1,1]
		N = normalize(Input.TBN * normalMapSample);
	} else {
		N = normalize(Input.Normal);
	}
	
	// Metallic
	float metallic = u_Metallic;
	if (u_UseMetallicMap != 0) {
		metallic = clamp(texture(u_MetallicMap, Input.TexCoords).r * u_MetallicMultiplier, 0.0, 1.0);
	}
	
	// Roughness
	float roughness = u_Roughness;
	if (u_UseRoughnessMap != 0) {
		roughness = clamp(texture(u_RoughnessMap, Input.TexCoords).r * u_RoughnessMultiplier, 0.0, 1.0);
	}
	// Clamp roughness to avoid artifacts
	roughness = max(roughness, 0.04);
	
	// Specular
	float specular = u_Specular;
	if (u_UseSpecularMap != 0) {
		specular = clamp(texture(u_SpecularMap, Input.TexCoords).r * u_SpecularMultiplier, 0.0, 1.0);
	}
	
	// Ambient Occlusion
	float ao = 1.0;
	if (u_UseAOMap != 0) {
		ao = clamp(texture(u_AOMap, Input.TexCoords).r * u_AOMultiplier, 0.0, 1.0);
	}
	
	// ========== PBR LIGHTING ==========
	
	vec3 V = normalize(u_ViewPos - Input.FragPos);
	
	// Calculate base reflectance (F0) for dielectrics (0.04) or metals (albedo)
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);
	
	// Apply specular intensity to F0 for non-metals
	F0 = mix(F0 * specular, F0, metallic);
	
	// Calculate per-light radiance
	vec3 Lo = CalculatePBRLighting(N, V, F0, albedo, metallic, roughness, Input.FragPos);
	
	// ========== AMBIENT LIGHTING (IBL approximation) ==========
	
	vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metallic;
	
	// Simple ambient with AO
	vec3 ambient = vec3(0.03) * albedo * ao * kD;
	
	// ========== EMISSION ==========
	
	vec3 emission = vec3(0.0);
	if (u_EmissionIntensity > EPSILON) {
		emission = u_EmissionColor * u_EmissionIntensity;
		if (u_UseEmissionMap != 0) {
			vec3 emissionSample = texture(u_EmissionMap, Input.TexCoords).rgb;
			emissionSample = pow(emissionSample, vec3(2.2)); // sRGB to linear
			emission = emissionSample * u_EmissionColor * u_EmissionIntensity;
		}
	}
	
	// ========== FINAL COLOR ==========
	
	vec3 color = ambient + Lo + emission;
	
	// HDR tonemapping (Reinhard)
	color = color / (color + vec3(1.0));
	
	// Gamma correction (linear to sRGB)
	color = pow(color, vec3(1.0 / 2.2));
	
	FragColor = vec4(color, alpha);
	o_EntityID = v_EntityID;
}

#endif