#version 450 core

// ============================================================================
// OUTLINE COMPOSITE SHADER
// Combines the blurred silhouette with the original silhouette to produce
// the final outline, then alpha-blends onto the scene.
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

layout(binding = 0) uniform sampler2D u_BlurredSilhouette;   // blurred mask
layout(binding = 1) uniform sampler2D u_OriginalSilhouette;  // original sharp mask

layout(std140, binding = 10) uniform CompositeParams {
	vec4 u_OutlineColor;             // outline color + alpha
	float u_OutlineHardness;         // 0 = soft/glow, 1 = hard edge
	float u_InsideAlpha;             // interior alpha (0 = transparent)
};

void main() {
	float blurred = texture(u_BlurredSilhouette, v_TexCoord).r;
	float original = texture(u_OriginalSilhouette, v_TexCoord).r;

	bool isInside = original > 0.5;

	float alpha;
	if (isInside) {
		// Inside the object: transparent or faint tint
		alpha = u_InsideAlpha;
	} else {
		// Outside: outline from blurred - original difference
		if (u_OutlineHardness > 0.99) {
			// Hard outline: step
			alpha = blurred > 0.01 ? 1.0 : 0.0;
		} else {
			// Soft outline with configurable hardness
			float edge = smoothstep(0.0, 1.0 - u_OutlineHardness, blurred);
			alpha = edge;
		}
	}

	// Discard fully transparent pixels
	if (alpha < 0.001) discard;

	FragColor = vec4(u_OutlineColor.rgb, alpha * u_OutlineColor.a);
}

#endif
