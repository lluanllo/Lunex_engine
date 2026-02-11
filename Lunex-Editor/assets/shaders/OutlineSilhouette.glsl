#version 450 core

// ============================================================================
// OUTLINE SILHOUETTE SHADER
// Renders objects as flat white silhouettes into the silhouette buffer.
// Used by OutlineRenderer for selection outlines and collider visualization.
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 8) uniform OutlineData {
	mat4 u_ViewProjection;
	mat4 u_Model;
	vec4 u_Color; // silhouette color (usually white)
};

void main() {
	gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;

layout(std140, binding = 8) uniform OutlineData {
	mat4 u_ViewProjection;
	mat4 u_Model;
	vec4 u_Color;
};

void main() {
	FragColor = u_Color;
}

#endif
