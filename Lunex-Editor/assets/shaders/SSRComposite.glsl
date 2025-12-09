#version 450 core

// ============================================================================
// SSR COMPOSITE SHADER
// Blends SSR result with scene color
// Only writes to color attachment (0), entity ID and normals are preserved
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 v_TexCoord;

layout(binding = 0) uniform sampler2D u_SceneColor;
layout(binding = 1) uniform sampler2D u_SSRResult;

layout(std140, binding = 7) uniform SSRCompositeData {
    float u_Intensity;
    float _padding1;
    float _padding2;
    float _padding3;
};

void main() {
    vec4 sceneColor = texture(u_SceneColor, v_TexCoord);
    vec4 ssrColor = texture(u_SSRResult, v_TexCoord);
    
    // SSR result already has intensity baked in
    // Just blend additively based on SSR alpha
    vec3 finalColor = mix(sceneColor.rgb, ssrColor.rgb, ssrColor.a * u_Intensity);
    
    FragColor = vec4(finalColor, sceneColor.a);
}

#endif
