#version 450 core

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;

// ? Use UBO for Vulkan compatibility
layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
};

layout(std140, binding = 1) uniform Transform {
    mat4 u_Transform;
};

void main() {
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(1.0, 1.0, 1.0, 1.0); // White mask for selected objects
}

#endif
