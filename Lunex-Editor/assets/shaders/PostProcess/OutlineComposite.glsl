#version 450 core

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

layout(binding = 0) uniform sampler2D u_EdgeMask;

layout(std140, binding = 1) uniform CompositeParams {
    vec4 u_OutlineColor;
    int u_BlendMode;     // 0=Alpha, 1=Additive, 2=Screen
    int u_DepthTest;
    float _padding1;
    float _padding2;
};

void main() {
    // Read edge intensity from mask
    float edge = texture(u_EdgeMask, v_TexCoord).r;
    
    // Output outline color modulated by edge intensity
    // Alpha blending will composite this on top of existing scene
    FragColor = vec4(u_OutlineColor.rgb, edge * u_OutlineColor.a);
}

#endif
