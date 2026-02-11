#version 450 core

// ============================================================================
// OUTLINE BLUR SHADER (Separable)
// Two-pass separable box blur for expanding the silhouette buffer.
// Pass 1: u_Direction = vec2(1,0) ? horizontal
// Pass 2: u_Direction = vec2(0,1) ? vertical
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main() {
	v_TexCoord = a_TexCoord;
	gl_Position = vec4(a_Position, 0.0, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_InputTexture;

layout(std140, binding = 9) uniform BlurParams {
	vec2 u_TexelSize;   // 1.0 / textureSize
	vec2 u_Direction;   // (1,0) for horizontal, (0,1) for vertical
	int u_KernelSize;   // blur radius in pixels
};

void main() {
	float sum = 0.0;
	int samples = 2 * u_KernelSize + 1;

	for (int i = 0; i < samples; i++) {
		float offset = float(i - u_KernelSize);
		vec2 uv = v_TexCoord + u_Direction * offset * u_TexelSize;
		sum += texture(u_InputTexture, uv).r;
	}

	FragColor = vec4(sum / float(samples), 0.0, 0.0, 1.0);
}

#endif
