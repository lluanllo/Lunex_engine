#version 450 core

// ============================================================================
// EQUIRECT TO CUBEMAP CONVERSION SHADER
// Converts equirectangular HDR to cubemap faces
// Compatible with Vulkan/SPIR-V
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;

layout(location = 0) out vec3 v_LocalPos;

// Uniforms in UBO for Vulkan compatibility
layout(std140, binding = 0) uniform CameraMatrices {
	mat4 u_Projection;
	mat4 u_View;
};

void main() {
	v_LocalPos = a_Position;
	gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec3 v_LocalPos;

// Sampler with explicit binding for Vulkan
layout(binding = 0) uniform sampler2D u_EquirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v) {
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

void main() {		
	vec2 uv = SampleSphericalMap(normalize(v_LocalPos));
	vec3 color = texture(u_EquirectangularMap, uv).rgb;
	
	FragColor = vec4(color, 1.0);
}

#endif
