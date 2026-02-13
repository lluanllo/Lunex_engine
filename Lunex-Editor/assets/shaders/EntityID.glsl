#version 450 core

// ============================================================================
// ENTITY ID PASS SHADER
//
// Minimal shader that renders mesh geometry writing ONLY the entity ID
// to the RED_INTEGER attachment (location 1).  Color output is discarded.
// Used by the path tracer backend so that mouse-picking still works:
// the PT computes color via compute shaders but cannot write entity IDs,
// so this fast raster pass fills in the ID buffer using the same camera.
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;      // unused, but must match Mesh vertex layout
layout(location = 2) in vec2 a_TexCoords;   // unused
layout(location = 3) in vec3 a_Tangent;      // unused
layout(location = 4) in vec3 a_Bitangent;    // unused
layout(location = 5) in int  a_EntityID;

layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
};

layout(std140, binding = 1) uniform Transform {
    mat4 u_Transform;
};

layout(location = 0) out flat int v_EntityID;

void main() {
    v_EntityID  = a_EntityID;
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

layout(location = 0) in flat int v_EntityID;

void main() {
    // Write a transparent color so the path-traced image is not overwritten.
    // The framebuffer's color blending is irrelevant here because we only
    // care about the integer attachment, but we must still output something.
    o_Color    = vec4(0.0, 0.0, 0.0, 0.0);
    o_EntityID = v_EntityID;
}

#endif
