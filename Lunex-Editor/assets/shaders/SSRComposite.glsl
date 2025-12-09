#version 450 core

// ============================================================================
// SSR COMPOSITE SHADER
// Blends SSR result with scene color using physically-based blending
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
    
    // SSR alpha represents confidence/coverage of the reflection
    float reflectionWeight = ssrColor.a * u_Intensity;
    
    // If no reflection, output scene color unchanged
    if (reflectionWeight < 0.001) {
        FragColor = sceneColor;
        return;
    }
    
    // Physically-based reflection blending:
    // Reflections add light to the surface (additive for dielectrics)
    // But for metals, reflections replace the base color (multiplicative/lerp)
    // Since we don't have metallic info here, use a hybrid approach:
    // - Low confidence SSR: additive (environment reflections)
    // - High confidence SSR: replacement (mirror reflections)
    
    // Additive component - adds reflection on top of scene
    vec3 additiveResult = sceneColor.rgb + ssrColor.rgb * reflectionWeight * 0.5;
    
    // Replacement component - blends reflection with scene
    vec3 replaceResult = mix(sceneColor.rgb, ssrColor.rgb, reflectionWeight * 0.7);
    
    // Blend between additive and replacement based on SSR confidence
    // High alpha (strong hit) = more replacement, low alpha = more additive
    vec3 finalColor = mix(additiveResult, replaceResult, ssrColor.a);
    
    // Energy conservation - prevent over-brightening
    // Allow up to 1.5x brightness for HDR headroom
    float maxBrightness = max(max(sceneColor.r, sceneColor.g), sceneColor.b) * 1.5 + 0.5;
    finalColor = min(finalColor, vec3(maxBrightness));
    
    FragColor = vec4(finalColor, sceneColor.a);
}

#endif
