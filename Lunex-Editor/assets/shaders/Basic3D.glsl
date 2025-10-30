#version 450 core

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

// Camera UBO (binding = 0)
layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	vec4 u_CameraPosition;
};

// Transform UBO (binding = 1)
layout(std140, binding = 1) uniform Transform
{
	mat4 u_Transform;
	mat4 u_NormalMatrix;
	int u_EntityID;
};

// Outputs - usando el mismo patrón que Renderer2D_Quad.glsl
struct VertexOutput {
	vec3 Position;
	vec3 Normal;
	vec2 TexCoord;
	vec3 Tangent;
	vec3 Bitangent;
};

layout (location = 0) out VertexOutput Output;
layout (location = 5) out flat int v_EntityID;

void main()
{
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	
	// Extract mat3 from mat4 for normal transformation
	mat3 normalMatrix = mat3(u_NormalMatrix);
	
	Output.Position = worldPos.xyz;
	Output.Normal = normalize(normalMatrix * a_Normal);
	Output.TexCoord = a_TexCoord;
	Output.Tangent = normalize(normalMatrix * a_Tangent);
	Output.Bitangent = normalize(normalMatrix * a_Bitangent);
	
	v_EntityID = u_EntityID;
	
	gl_Position = u_ViewProjection * worldPos;
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

// Inputs - mismo patrón que Renderer2D_Quad.glsl
struct VertexOutput {
	vec3 Position;
	vec3 Normal;
	vec2 TexCoord;
	vec3 Tangent;
	vec3 Bitangent;
};

layout (location = 0) in VertexOutput Input;
layout (location = 5) in flat int v_EntityID;

// Camera UBO
layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
	vec4 u_CameraPosition;
};

// ? Material UBO (binding = 2) - REQUERIDO para Vulkan
layout(std140, binding = 2) uniform MaterialBlock
{
	vec4 u_Material_Albedo_Metallic;  // xyz = Albedo, w = Metallic
	vec4 u_Material_Roughness_Emission_X; // x = Roughness, yzw = Emission
	vec4 u_Material_Flags;             // x = HasAlbedoMap, y = HasNormalMap, z = HasMetallicMap, w = HasRoughnessMap
};

// Samplers con explicit bindings
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
		vec3 tangentNormal = texture(u_Material_NormalMap, Input.TexCoord).xyz * 2.0 - 1.0;
		mat3 TBN = mat3(Input.Tangent, Input.Bitangent, Input.Normal);
		return normalize(TBN * tangentNormal);
	}
	return normalize(Input.Normal);
}

void main()
{
	// Get albedo
	vec3 albedo = u_Material_Albedo_Metallic.xyz;
	if (u_Material_Flags.x > 0.5)
	{
		albedo = texture(u_Material_AlbedoMap, Input.TexCoord).rgb;
	}

	// Get metallic/roughness
	float metallic = u_Material_Albedo_Metallic.w;
	float roughness = u_Material_Roughness_Emission_X.x;
	
	if (u_Material_Flags.z > 0.5)
	{
		metallic = texture(u_Material_MetallicMap, Input.TexCoord).r;
	}
	
	if (u_Material_Flags.w > 0.5)
	{
		roughness = texture(u_Material_RoughnessMap, Input.TexCoord).r;
	}

	// Get emission
	vec3 emission = u_Material_Roughness_Emission_X.yzw;

	// Get normal
	vec3 normal = GetNormal();

	// Simple lighting calculation (Blinn-Phong approximation)
	vec3 viewDir = normalize(u_CameraPosition.xyz - Input.Position);
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