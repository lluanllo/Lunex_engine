#version 450 core

// ============================================================================
// SHADOW DEPTH SHADER — Point Lights (Linear Depth)
//
// Point light shadows need linear depth because we sample by direction
// and compare linear distance, not projective depth.
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 6) in ivec4 a_BoneIDs;
layout(location = 7) in vec4  a_BoneWeights;

// Bone palette of the mesh being drawn (only read when u_Skinned != 0)
layout(std430, binding = 4) readonly buffer BoneMatrices {
	mat4 u_Bones[];
};

layout(std140, binding = 6) uniform ShadowDepthPointData {
	mat4 u_LightVP;
	mat4 u_Model;
	vec4 u_LightPosAndRange; // xyz = position, w = range
	int  u_Skinned;
};

mat4 GetSkinMatrix() {
	if (u_Skinned == 0) return mat4(1.0);
	return a_BoneWeights.x * u_Bones[a_BoneIDs.x] +
	       a_BoneWeights.y * u_Bones[a_BoneIDs.y] +
	       a_BoneWeights.z * u_Bones[a_BoneIDs.z] +
	       a_BoneWeights.w * u_Bones[a_BoneIDs.w];
}

layout(location = 0) out vec3 v_FragPos;

void main() {
	vec4 worldPos = u_Model * (GetSkinMatrix() * vec4(a_Position, 1.0));
	v_FragPos = worldPos.xyz;
	gl_Position = u_LightVP * worldPos;
}

#elif defined(FRAGMENT)

layout(location = 0) in vec3 v_FragPos;

layout(std140, binding = 6) uniform ShadowDepthPointData {
	mat4 u_LightVP;
	mat4 u_Model;
	vec4 u_LightPosAndRange; // xyz = position, w = range
};

void main() {
	// Write linear depth normalized to [0, 1] range
	float dist = length(v_FragPos - u_LightPosAndRange.xyz);
	float range = u_LightPosAndRange.w;
	
	// Clamp to valid range to avoid artifacts at boundaries
	gl_FragDepth = clamp(dist / range, 0.0, 1.0);
}

#endif
