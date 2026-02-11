#version 450 core

// ============================================================================
// SHADOW DEPTH SHADER — Directional & Spot lights
// Minimal: only transforms vertices and writes depth.
// No material evaluation, no lighting, no color output.
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
// We only need position for depth rendering.
// Other attributes are ignored.

layout(std140, binding = 6) uniform ShadowDepthData {
	mat4 u_LightVP;
	mat4 u_Model;
};

void main() {
	gl_Position = u_LightVP * u_Model * vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

// Fragment shader is empty for directional/spot shadows.
// Depth is written automatically by the fixed-function pipeline.
void main() {
	// gl_FragDepth is written automatically
}

#endif
