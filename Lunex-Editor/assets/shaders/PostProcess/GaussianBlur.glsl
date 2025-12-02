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

uniform sampler2D u_Texture;
uniform vec2 u_Direction;    // (1,0) horizontal, (0,1) vertical
uniform float u_BlurRadius;  // Kernel size
uniform vec2 u_TexelSize;    // 1.0 / textureSize

void main() {
    vec4 result = vec4(0.0);
    float totalWeight = 0.0;
    
    int kernelSize = int(ceil(u_BlurRadius));
    
    for (int i = -kernelSize; i <= kernelSize; i++) {
        vec2 offset = u_Direction * float(i) * u_TexelSize;
        float weight = exp(-0.5 * pow(float(i) / u_BlurRadius, 2.0));
        
        result += texture(u_Texture, v_TexCoord + offset) * weight;
        totalWeight += weight;
    }
    
    FragColor = result / totalWeight;
}

#endif
