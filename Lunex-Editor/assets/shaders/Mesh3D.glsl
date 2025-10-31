#version 460 core

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

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

void main() {
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	Output.FragPos = worldPos.xyz;
	Output.Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
	Output.TexCoords = a_TexCoords;
	
	gl_Position = u_ViewProjection * worldPos;
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
};

layout (location = 0) in VertexOutput Input;

layout(std140, binding = 2) uniform Material {
	vec4 u_Color;
	vec3 u_LightPos;
	vec3 u_ViewPos;
	vec3 u_LightColor;
	bool u_UseTexture;
};

layout (binding = 0) uniform sampler2D texture_diffuse1;

void main() {
	// Ambient
	float ambientStrength = 0.2;
	vec3 ambient = ambientStrength * u_LightColor;
	
	// Diffuse
	vec3 norm = normalize(Input.Normal);
	vec3 lightDir = normalize(u_LightPos - Input.FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * u_LightColor;
	
	// Specular
	float specularStrength = 0.5;
	vec3 viewDir = normalize(u_ViewPos - Input.FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
	vec3 specular = specularStrength * spec * u_LightColor;
	
	vec3 result;
	if (u_UseTexture) {
		vec4 texColor = texture(texture_diffuse1, Input.TexCoords);
		result = (ambient + diffuse + specular) * texColor.rgb;
	} else {
		result = (ambient + diffuse + specular) * u_Color.rgb;
	}
	
	FragColor = vec4(result, u_Color.a);
}

#endif
