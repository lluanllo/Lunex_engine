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
layout(location = 1) out int o_EntityID;  // ✅ SIN "flat"

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
};

layout (location = 0) in VertexOutput Input;
layout (location = 3) in flat int v_EntityID;

layout(std140, binding = 2) uniform Material {
	vec4 u_Color;
	vec3 u_LightPos;
	float _padding1;
	vec3 u_ViewPos;
	float _padding2;
	vec3 u_LightColor;
	int u_UseTexture;
};

layout (binding = 0) uniform sampler2D texture_diffuse1;

void main() {
	float ambientStrength = 0.2;
	vec3 ambient = ambientStrength * u_LightColor;
	
	vec3 norm = normalize(Input.Normal);
	vec3 lightDir = normalize(u_LightPos - Input.FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * u_LightColor;
	
	float specularStrength = 0.5;
	vec3 viewDir = normalize(u_ViewPos - Input.FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
	vec3 specular = specularStrength * spec * u_LightColor;
	
	vec3 result;
	if (u_UseTexture != 0) {
		vec4 texColor = texture(texture_diffuse1, Input.TexCoords);
		result = (ambient + diffuse + specular) * texColor.rgb;
	} else {
		result = (ambient + diffuse + specular) * u_Color.rgb;
	}
	
	FragColor = vec4(result, u_Color.a);
	o_EntityID = v_EntityID;  // ✅ Escribir EntityID
}

#endif