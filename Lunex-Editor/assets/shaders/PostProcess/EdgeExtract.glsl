#version 330 core

#ifdef VERTEX

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#elif defined(FRAGMENT)

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_BlurredMask;
uniform sampler2D u_OriginalMask;

void main() {
    float blurred = texture(u_BlurredMask, v_TexCoord).r;
    float original = texture(u_OriginalMask, v_TexCoord).r;
    
    // Edge = blurred - original (outline around the object)
    float edge = blurred - original;
    edge = max(edge, 0.0); // Clamp negative values
    
    FragColor = vec4(edge, edge, edge, edge);
}

#endif
