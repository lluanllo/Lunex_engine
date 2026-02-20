#version 450 core

/**
 * PostProcess - Bloom Upsample
 * 
 * 9-tap tent filter for smooth upsampling.
 * Used additively to blend bloom into the next higher mip.
 */

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoords;

layout(location = 0) out vec2 v_TexCoords;

void main() {
	v_TexCoords = a_TexCoords;
	gl_Position = vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec3 FragColor;

layout(location = 0) in vec2 v_TexCoords;

layout(binding = 0) uniform sampler2D u_SrcTexture;

layout(std140, binding = 0) uniform BloomUpParams {
	float u_FilterRadius;
	float _pad1;
	float _pad2;
	float _pad3;
};

// 9-tap tent filter (3x3 bilinear taps)
vec3 UpsampleTent9(sampler2D tex, vec2 uv, vec2 texelSize, float radius) {
	vec2 d = texelSize * radius;

	vec3 a = texture(tex, uv + vec2(-d.x,  d.y)).rgb;
	vec3 b = texture(tex, uv + vec2( 0.0,  d.y)).rgb;
	vec3 c = texture(tex, uv + vec2( d.x,  d.y)).rgb;

	vec3 d2 = texture(tex, uv + vec2(-d.x, 0.0)).rgb;
	vec3 e = texture(tex, uv).rgb;
	vec3 f = texture(tex, uv + vec2( d.x, 0.0)).rgb;

	vec3 g = texture(tex, uv + vec2(-d.x, -d.y)).rgb;
	vec3 h = texture(tex, uv + vec2( 0.0, -d.y)).rgb;
	vec3 i = texture(tex, uv + vec2( d.x, -d.y)).rgb;

	// Tent weights: corners=1, edges=2, center=4 -> total=16
	vec3 result = e * 4.0;
	result += (b + d2 + f + h) * 2.0;
	result += (a + c + g + i);
	return result * (1.0 / 16.0);
}

void main() {
	vec2 texelSize = 1.0 / vec2(textureSize(u_SrcTexture, 0));
	FragColor = UpsampleTent9(u_SrcTexture, v_TexCoords, texelSize, u_FilterRadius);
}

#endif
