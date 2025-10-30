#version 460 core

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

// Camera UBO (binding = 4) - moved to avoid conflicts with 2D
layout(std140, binding = 4) uniform Camera
{
	mat4 u_ViewProjection;
	vec4 u_CameraPosition;
};

// Transform UBO (binding = 5)
layout(std140, binding = 5) uniform Transform
{
	mat4 u_Transform;
	mat4 u_NormalMatrix;
	int u_EntityID;
};

// Outputs - usar variables individuales en lugar de struct
layout(location = 0) out vec3 v_Position;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out vec3 v_Tangent;
layout(location = 4) out vec3 v_Bitangent;
layout(location = 5) out flat int v_EntityID;

void main()
{
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	
	mat3 normalMatrix = mat3(u_NormalMatrix);
	
	v_Position = worldPos.xyz;
	v_Normal = normalize(normalMatrix * a_Normal);
	v_TexCoord = a_TexCoord;
	v_Tangent = normalize(normalMatrix * a_Tangent);
	v_Bitangent = normalize(normalMatrix * a_Bitangent);
	v_EntityID = u_EntityID;
	
	gl_Position = u_ViewProjection * worldPos;
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

// Inputs - usar variables individuales
layout(location = 0) in vec3 v_Position;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec3 v_Tangent;
layout(location = 4) in vec3 v_Bitangent;
layout(location = 5) in flat int v_EntityID;

// Camera UBO (binding = 4)
layout(std140, binding = 4) uniform Camera
{
	mat4 u_ViewProjection;
	vec4 u_CameraPosition;
};

// Material UBO (binding = 6)
layout(std140, binding = 6) uniform MaterialBlock
{
	vec4 u_Material_Albedo_Metallic;
	vec4 u_Material_Roughness_Emission_X;
	vec4 u_Material_Flags;
};

// Samplers
layout(binding = 0) uniform sampler2D u_Material_AlbedoMap;
layout(binding = 1) uniform sampler2D u_Material_NormalMap;
layout(binding = 2) uniform sampler2D u_Material_MetallicMap;
layout(binding = 3) uniform sampler2D u_Material_RoughnessMap;

// Simple directional light
const vec3 lightDir = normalize(vec3(0.5, -1.0, 0.3));
const vec3 lightColor = vec3(1.0, 1.0, 1.0);
const float ambientStrength = 0.3;

vec3 GetNormal()
{
	if (u_Material_Flags.y > 0.5)
	{
		vec3 tangentNormal = texture(u_Material_NormalMap, v_TexCoord).xyz * 2.0 - 1.0;
		mat3 TBN = mat3(v_Tangent, v_Bitangent, v_Normal);
		return normalize(TBN * tangentNormal);
	}
	return normalize(v_Normal);
}

void main()
{
	// Get albedo
	vec3 albedo = u_Material_Albedo_Metallic.xyz;
	if (u_Material_Flags.x > 0.5)
	{
		albedo = texture(u_Material_AlbedoMap, v_TexCoord).rgb;
	}

	// Get metallic/roughness
	float metallic = u_Material_Albedo_Metallic.w;
	float roughness = u_Material_Roughness_Emission_X.x;
	
	if (u_Material_Flags.z > 0.5)
	{
		metallic = texture(u_Material_MetallicMap, v_TexCoord).r;
	}
	
	if (u_Material_Flags.w > 0.5)
	{
		roughness = texture(u_Material_RoughnessMap, v_TexCoord).r;
	}

	// Get emission
	vec3 emission = u_Material_Roughness_Emission_X.yzw;

	// Get normal
	vec3 normal = GetNormal();

	// Simple lighting calculation (Blinn-Phong approximation)
	vec3 viewDir = normalize(u_CameraPosition.xyz - v_Position);
	vec3 halfDir = normalize(-lightDir + viewDir);

	// Diffuse
	float diff = max(dot(normal, -lightDir), 0.0);
	vec3 diffuse = diff * lightColor;

	// Specular (modified by roughness)
	float spec = pow(max(dot(normal, halfDir), 0.0), 32.0 * (1.0 - roughness));
	vec3 specular = spec * lightColor * (1.0 - roughness);

	// Ambient
	vec3 ambient = ambientStrength * lightColor;

	// Combine
	vec3 lighting = ambient + diffuse + specular;
	vec3 color = albedo * lighting + emission;

	// Output
	o_Color = vec4(color, 1.0);
	o_EntityID = v_EntityID;
}

#endif
