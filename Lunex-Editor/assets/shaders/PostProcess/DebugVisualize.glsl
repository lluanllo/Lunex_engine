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

layout(binding = 0) uniform sampler2D u_Texture;

void main() {
    // Visualize texture directly (red channel for mask)
    float value = texture(u_Texture, v_TexCoord).r;
    FragColor = vec4(value, value, value, 1.0);
}

#endif
