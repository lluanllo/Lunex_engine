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
};

layout (location = 0) out VertexOutput Output;
layout (location = 3) out flat int v_EntityID;

void main() {
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	Output.FragPos = worldPos.xyz;
	Output.Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
	Output.TexCoords = a_TexCoords;
	
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
};

layout (location = 0) in VertexOutput Input;
layout (location = 3) in flat int v_EntityID;

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
	int u_UseTexture;
};

// Lights array
#define MAX_LIGHTS 16

struct LightData {
	vec4 Position;      // xyz = position, w = type (0=Dir, 1=Point, 2=Spot)
	vec4 Direction;     // xyz = direction, w = unused
	vec4 Color;         // rgb = color, a = intensity
	vec4 Params;        // x = range, y = innerCone, z = outerCone, w = castShadows
	vec4 Attenuation;   // xyz = constant/linear/quadratic, w = unused
};

layout(std140, binding = 3) uniform Lights {
	int u_NumLights;
	float _padding2;
	float _padding3;
	float _padding4;
	LightData u_Lights[MAX_LIGHTS];
};

layout (binding = 0) uniform sampler2D texture_diffuse1;

// Calculate light contribution
vec3 CalculateLighting(vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor) {
	vec3 totalLight = vec3(0.0);
	
	for (int i = 0; i < u_NumLights && i < MAX_LIGHTS; i++) {
		LightData light = u_Lights[i];
		int lightType = int(light.Position.w);
		
		vec3 lightDir;
		float attenuation = 1.0;
		
		// Directional Light
		if (lightType == 0) {
			lightDir = normalize(-light.Direction.xyz);
		}
		// Point Light
		else if (lightType == 1) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			lightDir = normalize(lightVec);
			
			// Attenuation
			float constant = light.Attenuation.x;
			float linear = light.Attenuation.y;
			float quadratic = light.Attenuation.z;
			attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
			
			// Range check
			if (distance > light.Params.x) {
				attenuation = 0.0;
			}
		}
		// Spot Light
		else if (lightType == 2) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			lightDir = normalize(lightVec);
			
			// Attenuation
			float constant = light.Attenuation.x;
			float linear = light.Attenuation.y;
			float quadratic = light.Attenuation.z;
			attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
			
			// Range check
			if (distance > light.Params.x) {
				attenuation = 0.0;
			}
			
			// Spot cone
			float theta = dot(lightDir, normalize(-light.Direction.xyz));
			float innerCone = light.Params.y;
			float outerCone = light.Params.z;
			float epsilon = innerCone - outerCone;
			float intensity = clamp((theta - outerCone) / epsilon, 0.0, 1.0);
			attenuation *= intensity;
		}
		
		// Skip if no contribution
		if (attenuation <= 0.0) continue;
		
		// Diffuse
		float diff = max(dot(normal, lightDir), 0.0);
		vec3 diffuse = diff * light.Color.rgb * light.Color.a;
		
		// Specular (Blinn-Phong)
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float shininess = mix(2.0, 128.0, 1.0 - u_Roughness);
		float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
		vec3 specular = u_Specular * spec * light.Color.rgb * light.Color.a;
		
		// Apply metallic effect
		vec3 F0 = mix(vec3(0.04), baseColor, u_Metallic);
		specular *= F0;
		
		// Combine and apply attenuation
		totalLight += (diffuse + specular) * attenuation;
	}
	
	return totalLight;
}

void main() {
	// Get base color
	vec3 baseColor;
	if (u_UseTexture != 0) {
		vec4 texColor = texture(texture_diffuse1, Input.TexCoords);
		baseColor = texColor.rgb * u_Color.rgb;
	} else {
		baseColor = u_Color.rgb;
	}
	
	// Normalize inputs
	vec3 norm = normalize(Input.Normal);
	vec3 viewDir = normalize(u_ViewPos - Input.FragPos);
	
	// Ambient light (simple global ambient)
	vec3 ambient = vec3(0.1) * baseColor;
	
	// Calculate lighting from all lights
	vec3 lighting = CalculateLighting(norm, Input.FragPos, viewDir, baseColor);
	
	// Combine
	vec3 result = ambient + lighting * baseColor;
	
	// Add emission
	result += u_EmissionColor * u_EmissionIntensity;
	
	FragColor = vec4(result, u_Color.a);
	o_EntityID = v_EntityID;
}

#endif