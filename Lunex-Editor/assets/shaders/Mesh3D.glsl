#version 450 core

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

// ARREGLADO: Cambiado bool por int y ajustado padding std140
layout(std140, binding = 2) uniform Material {
	vec4 u_Color;          // 16 bytes, offset 0
	vec3 u_LightPos;       // 12 bytes, offset 16
	float _padding1;       // 4 bytes, offset 28 (completa a 32)
	vec3 u_ViewPos;        // 12 bytes, offset 32
	float _padding2;       // 4 bytes, offset 44 (completa a 48)
	vec3 u_LightColor;     // 12 bytes, offset 48
	int u_UseTexture;      // 4 bytes, offset 60 (cambiado de bool a int)
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
	if (u_UseTexture != 0) {  // CAMBIADO: comparación con int en lugar de bool
		vec4 texColor = texture(texture_diffuse1, Input.TexCoords);
		result = (ambient + diffuse + specular) * texColor.rgb;
	} else {
		result = (ambient + diffuse + specular) * u_Color.rgb;
	}
	
	FragColor = vec4(result, u_Color.a);
}

#endif