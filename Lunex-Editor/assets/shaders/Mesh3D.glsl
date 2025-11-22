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
	
	mat3 normalMatrix = mat3(transpose(inverse(u_Transform)));
	Output.Normal = normalize(normalMatrix * a_Normal);
	Output.TexCoords = a_TexCoords;
	
	// Calculate TBN matrix for normal mapping
	vec3 T = normalize(normalMatrix * a_Tangent);
	vec3 N = Output.Normal;
	T = normalize(T - dot(T, N) * N);
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

// Material properties
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

// Lights array
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

// Texture samplers
layout (binding = 0) uniform sampler2D u_AlbedoMap;
layout (binding = 1) uniform sampler2D u_NormalMap;
layout (binding = 2) uniform sampler2D u_MetallicMap;
layout (binding = 3) uniform sampler2D u_RoughnessMap;
layout (binding = 4) uniform sampler2D u_SpecularMap;
layout (binding = 5) uniform sampler2D u_EmissionMap;
layout (binding = 6) uniform sampler2D u_AOMap;

// Calculate light contribution
vec3 CalculateLighting(vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor, float metallic, float roughness, float specular) {
	vec3 totalLight = vec3(0.0);
	
	for (int i = 0; i < u_NumLights && i < MAX_LIGHTS; i++) {
		LightData light = u_Lights[i];
		int lightType = int(light.Position.w);
		
		vec3 lightDir;
		float attenuation = 1.0;
		
		if (lightType == 0) {
			lightDir = normalize(-light.Direction.xyz);
		}
		else if (lightType == 1) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			lightDir = normalize(lightVec);
			
			float constant = light.Attenuation.x;
			float linear = light.Attenuation.y;
			float quadratic = light.Attenuation.z;
			attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
			
			if (distance > light.Params.x) {
				attenuation = 0.0;
			}
		}
		else if (lightType == 2) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			lightDir = normalize(lightVec);
			
			float constant = light.Attenuation.x;
			float linear = light.Attenuation.y;
			float quadratic = light.Attenuation.z;
			attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
			
			if (distance > light.Params.x) {
				attenuation = 0.0;
			}
			
			float theta = dot(lightDir, normalize(-light.Direction.xyz));
			float innerCone = light.Params.y;
			float outerCone = light.Params.z;
			float epsilon = innerCone - outerCone;
			float intensity = clamp((theta - outerCone) / epsilon, 0.0, 1.0);
			attenuation *= intensity;
		}
		
		if (attenuation <= 0.0) continue;
		
		// Diffuse
		float diff = max(dot(normal, lightDir), 0.0);
		vec3 diffuse = diff * light.Color.rgb * light.Color.a;
		
		// Specular (Blinn-Phong)
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float shininess = mix(2.0, 128.0, 1.0 - roughness);
		float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
		vec3 specularContrib = specular * spec * light.Color.rgb * light.Color.a;
		
		// Apply metallic effect
		vec3 F0 = mix(vec3(0.04), baseColor, metallic);
		specularContrib *= F0;
		
		totalLight += (diffuse + specularContrib) * attenuation;
	}
	
	return totalLight;
}

void main() {
	// Sample albedo
	vec3 albedo;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, Input.TexCoords);
		albedo = texColor.rgb;
	} else {
		albedo = u_Color.rgb;
	}
	
	// Sample and apply normal map
	vec3 normal;
	if (u_UseNormalMap != 0) {
		vec3 normalMapSample = texture(u_NormalMap, Input.TexCoords).rgb;
		normalMapSample = normalMapSample * 2.0 - 1.0;
		normal = normalize(Input.TBN * normalMapSample);
	} else {
		normal = normalize(Input.Normal);
	}
	
	// Sample metallic
	float metallic = u_Metallic;
	if (u_UseMetallicMap != 0) {
		float metallicSample = texture(u_MetallicMap, Input.TexCoords).r;
		metallic = metallicSample * u_MetallicMultiplier;
	}
	
	// Sample roughness
	float roughness = u_Roughness;
	if (u_UseRoughnessMap != 0) {
		float roughnessSample = texture(u_RoughnessMap, Input.TexCoords).r;
		roughness = roughnessSample * u_RoughnessMultiplier;
	}
	
	// Sample specular
	float specular = u_Specular;
	if (u_UseSpecularMap != 0) {
		float specularSample = texture(u_SpecularMap, Input.TexCoords).r;
		specular = specularSample * u_SpecularMultiplier;
	}
	
	// Sample AO
	float ao = 1.0;
	if (u_UseAOMap != 0) {
		ao = texture(u_AOMap, Input.TexCoords).r * u_AOMultiplier;
	}
	
	vec3 viewDir = normalize(u_ViewPos - Input.FragPos);
	
	// Ambient light with AO
	vec3 ambient = vec3(0.1) * albedo * ao;
	
	// Calculate lighting
	vec3 lighting = CalculateLighting(normal, Input.FragPos, viewDir, albedo, metallic, roughness, specular);
	
	// Combine
	vec3 result = ambient + lighting * albedo;
	
	// Sample and add emission
	vec3 emission = u_EmissionColor * u_EmissionIntensity;
	if (u_UseEmissionMap != 0) {
		vec3 emissionSample = texture(u_EmissionMap, Input.TexCoords).rgb;
		emission = emissionSample * u_EmissionColor * u_EmissionIntensity;
	}
	result += emission;
	
	FragColor = vec4(result, u_Color.a);
	o_EntityID = v_EntityID;
}

#endif