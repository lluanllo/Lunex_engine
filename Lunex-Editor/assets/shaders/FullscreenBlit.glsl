#version 450 core

// ============================================================================
// FULLSCREEN BLIT SHADER
//
// Draws the path tracer output texture as a fullscreen quad onto the
// currently-bound framebuffer.  Writes entity-ID = -1 so the pixel is
// treated as "no entity" during mouse picking.
// ============================================================================

#ifdef VERTEX

layout(location = 0) out vec2 v_TexCoord;

void main() {
    // Full-screen triangle trick (3 vertices, no VBO needed)
    //   gl_VertexID  0 ? (-1,-1)   uv (0,0)
    //   gl_VertexID  1 ? ( 3,-1)   uv (2,0)
    //   gl_VertexID  2 ? (-1, 3)   uv (0,2)
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    v_TexCoord = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

layout(binding = 0) uniform sampler2D u_SourceTexture;

void main() {
    o_Color    = texture(u_SourceTexture, v_TexCoord);
    o_EntityID = -1;
}

#endif
