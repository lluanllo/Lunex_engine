#version 450 core

// ============================================================================
// SHADOW DEPTH SHADER — Directional & Spot lights
// Minimal: only transforms vertices and writes depth.
// No material evaluation, no lighting, no color output.
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 6) in ivec4 a_BoneIDs;
layout(location = 7) in vec4  a_BoneWeights;

// Bone palette of the mesh being drawn (only read when u_Skinned != 0)
layout(std430, binding = 4) readonly buffer BoneMatrices {
	mat4 u_Bones[];
};

layout(std140, binding = 6) uniform ShadowDepthData {
	mat4 u_LightVP;
	mat4 u_Model;
	vec4 _lightPosAndRange;   // used by the point-light variant
	int  u_Skinned;
};

mat4 GetSkinMatrix() {
	if (u_Skinned == 0) return mat4(1.0);
	return a_BoneWeights.x * u_Bones[a_BoneIDs.x] +
	       a_BoneWeights.y * u_Bones[a_BoneIDs.y] +
	       a_BoneWeights.z * u_Bones[a_BoneIDs.z] +
	       a_BoneWeights.w * u_Bones[a_BoneIDs.w];
}

void main() {
	gl_Position = u_LightVP * u_Model * (GetSkinMatrix() * vec4(a_Position, 1.0));
}

#elif defined(FRAGMENT)

// Fragment shader is empty for directional/spot shadows.
// Depth is written automatically by the fixed-function pipeline.
void main() {
	// gl_FragDepth is written automatically
}

#endif
