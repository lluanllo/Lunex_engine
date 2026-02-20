#version 450 core

/**
 * PostProcess - Bloom Downsample
 * 
 * Progressive downsampling with 13-tap filter (from Call of Duty: Advanced Warfare).
 * First pass applies a brightness threshold to extract bright areas.
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

uniform sampler2D u_SrcTexture;
uniform vec2 u_SrcResolution;
uniform float u_Threshold;
uniform int u_ApplyThreshold;

// 13-tap downsample filter (CoD:AW style - energy conserving)
vec3 DownsampleBox13(sampler2D tex, vec2 uv, vec2 texelSize) {
	// Center
	vec3 A = texture(tex, uv).rgb;

	// Inner box (4 samples)
	vec3 B = texture(tex, uv + texelSize * vec2(-1.0, -1.0)).rgb;
	vec3 C = texture(tex, uv + texelSize * vec2( 1.0, -1.0)).rgb;
	vec3 D = texture(tex, uv + texelSize * vec2(-1.0,  1.0)).rgb;
	vec3 E = texture(tex, uv + texelSize * vec2( 1.0,  1.0)).rgb;

	// Outer ring (8 samples)
	vec3 F = texture(tex, uv + texelSize * vec2(-2.0, -2.0)).rgb;
	vec3 G = texture(tex, uv + texelSize * vec2( 0.0, -2.0)).rgb;
	vec3 H = texture(tex, uv + texelSize * vec2( 2.0, -2.0)).rgb;
	vec3 I = texture(tex, uv + texelSize * vec2(-2.0,  0.0)).rgb;
	vec3 J = texture(tex, uv + texelSize * vec2( 2.0,  0.0)).rgb;
	vec3 K = texture(tex, uv + texelSize * vec2(-2.0,  2.0)).rgb;
	vec3 L = texture(tex, uv + texelSize * vec2( 0.0,  2.0)).rgb;
	vec3 M = texture(tex, uv + texelSize * vec2( 2.0,  2.0)).rgb;

	// Weighted combination (total weight = 1.0)
	vec3 result = vec3(0.0);
	// Inner: 0.5 weight distributed among 5 samples
	result += A * 0.125;
	result += (B + C + D + E) * 0.03125;  // 4 * 0.03125 = 0.125
	// Cross: 4 samples at edges of inner box, each sampled at half-offset
	result += (B + C + D + E) * 0.0625;   // 4 * 0.0625 = 0.25
	// Outer corners
	result += (F + H + K + M) * 0.03125;  // 4 * 0.03125 = 0.125
	// Outer edges
	result += (G + I + J + L) * 0.0625;   // 4 * 0.0625 = 0.25

	return result;
}

// Soft threshold (knee curve) to avoid harsh cutoff
vec3 ApplyThreshold(vec3 color, float threshold) {
	float brightness = max(color.r, max(color.g, color.b));
	float knee = threshold * 0.5;
	float soft = brightness - threshold + knee;
	soft = clamp(soft, 0.0, 2.0 * knee);
	soft = soft * soft / (4.0 * knee + 0.00001);
	float contribution = max(soft, brightness - threshold);
	contribution /= max(brightness, 0.00001);
	return color * max(contribution, 0.0);
}

void main() {
	vec2 texelSize = 1.0 / u_SrcResolution;
	vec3 color = DownsampleBox13(u_SrcTexture, v_TexCoords, texelSize);

	if (u_ApplyThreshold != 0) {
		color = ApplyThreshold(color, u_Threshold);
	}

	FragColor = color;
}

#endif
