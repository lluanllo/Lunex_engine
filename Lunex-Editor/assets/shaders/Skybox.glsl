#version 450 core

// ============================================================================
// SKYBOX SHADER
// 
// Renders a cubemap as a skybox background.
// Features:
//   - Depth trick to render at infinite distance
//   - EntityID = -1 to prevent selection picking
//   - Rotation support
//   - Intensity/tint control
//   - Blur (LOD) support for different mip levels
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
};

layout(std140, binding = 4) uniform SkyboxData {
	mat4 u_ViewRotation;  // View matrix with position removed (rotation only)
	mat4 u_Projection;
	float u_Intensity;
	float u_Rotation;     // Rotation around Y axis in radians
	float u_Blur;         // 0 = sharp, 1 = max blur (mip level)
	float u_MaxMipLevel;
	vec3 u_Tint;
	float _padding;
};

layout(location = 0) out vec3 v_TexCoords;

void main() {
	v_TexCoords = a_Position;
	
	// Apply rotation around Y axis
	float cosR = cos(u_Rotation);
	float sinR = sin(u_Rotation);
	mat3 rotY = mat3(
		cosR, 0.0, sinR,
		0.0, 1.0, 0.0,
		-sinR, 0.0, cosR
	);
	v_TexCoords = rotY * a_Position;
	
	// Remove translation from view matrix (skybox follows camera)
	mat4 viewNoTranslation = mat4(mat3(u_ViewRotation));
	vec4 pos = u_Projection * viewNoTranslation * vec4(a_Position, 1.0);
	
	// Set depth to 1.0 (farthest possible) so skybox is always behind everything
	gl_Position = pos.xyww;
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

layout(location = 0) in vec3 v_TexCoords;

layout(std140, binding = 4) uniform SkyboxData {
	mat4 u_ViewRotation;
	mat4 u_Projection;
	float u_Intensity;
	float u_Rotation;
	float u_Blur;
	float u_MaxMipLevel;
	vec3 u_Tint;
	float _padding;
};

layout(binding = 7) uniform samplerCube u_EnvironmentMap;

// ACES tone mapping
vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
	// Sample cubemap with optional blur (LOD)
	float lod = u_Blur * u_MaxMipLevel;
	vec3 envColor = textureLod(u_EnvironmentMap, v_TexCoords, lod).rgb;
	
	// Apply intensity and tint
	envColor *= u_Intensity * u_Tint;
	
	// Tone mapping (HDR to LDR)
	envColor = ACESFilm(envColor);
	
	// Gamma correction
	envColor = pow(envColor, vec3(1.0 / 2.2));
	
	o_Color = vec4(envColor, 1.0);
	
	// EntityID = -1 means this won't be picked by mouse selection
	o_EntityID = -1;
}

#endif
