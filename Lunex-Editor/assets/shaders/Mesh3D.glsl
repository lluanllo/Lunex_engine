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

layout(std140, binding = 2) uniform Material {
	vec4 u_Color;
	float u_Metallic;
	float u_Roughness;
	float u_Specular;
	float u_EmissionIntensity;
	vec3 u_EmissionColor;
	float _padding1;
	vec3 u_LightPos;
	float _padding2;
	vec3 u_ViewPos;
	float _padding3;
	vec3 u_LightColor;
	int u_UseTexture;
};

layout (binding = 0) uniform sampler2D texture_diffuse1;

void main() {
	// Basic lighting calculation (will be enhanced with full PBR later)
	float ambientStrength = 0.2;
	vec3 ambient = ambientStrength * u_LightColor;
	
	vec3 norm = normalize(Input.Normal);
	vec3 lightDir = normalize(u_LightPos - Input.FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * u_LightColor;
	
	// Specular with material specular property
	float specularStrength = u_Specular;
	vec3 viewDir = normalize(u_ViewPos - Input.FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), mix(2.0, 128.0, 1.0 - u_Roughness));
	vec3 specular = specularStrength * spec * u_LightColor;
	
	// Apply metallic (simple lerp for now)
	vec3 baseColor;
	if (u_UseTexture != 0) {
		vec4 texColor = texture(texture_diffuse1, Input.TexCoords);
		baseColor = texColor.rgb;
	} else {
		baseColor = u_Color.rgb;
	}
	
	// Simple metallic effect
	vec3 result = mix(
		(ambient + diffuse + specular) * baseColor,
		(ambient * 0.5 + diffuse + specular * 2.0) * baseColor,
		u_Metallic
	);
	
	// Add emission
	result += u_EmissionColor * u_EmissionIntensity;
	
	FragColor = vec4(result, u_Color.a);
	o_EntityID = v_EntityID;
}

#endif