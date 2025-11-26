#version 450 core

// ============================================================================
// SHADOW DENOISING COMPUTE SHADER
// Applies bilateral filtering to smooth shadow edges
// ============================================================================

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// ============================================================================
// INPUTS & OUTPUTS
// ============================================================================

// Input shadow map (noisy)
layout(binding = 0) uniform sampler2D u_ShadowInput;

// G-Buffer for edge-aware filtering
layout(binding = 1) uniform sampler2D u_GBufferNormal;
layout(binding = 2) uniform sampler2D u_GBufferDepth;

// Output (denoised)
layout(rgba16f, binding = 3) uniform writeonly image2D u_ShadowOutput;

// ============================================================================
// UNIFORMS
// ============================================================================

layout(location = 0) uniform ivec2 u_Resolution;
layout(location = 1) uniform float u_FilterRadius;      // Pixels to sample
layout(location = 2) uniform float u_DepthThreshold;    // Depth similarity threshold
layout(location = 3) uniform float u_NormalThreshold;   // Normal similarity threshold

// ============================================================================
// BILATERAL FILTER
// ============================================================================

void main() {
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    if (pixelCoords.x >= u_Resolution.x || pixelCoords.y >= u_Resolution.y)
        return;
    
    vec2 uv = vec2(pixelCoords) / vec2(u_Resolution);
    vec2 texelSize = 1.0 / vec2(u_Resolution);
    
    // Center pixel values
    float centerShadow = texture(u_ShadowInput, uv).r;
    vec3 centerNormal = texture(u_GBufferNormal, uv).rgb;
    float centerDepth = texture(u_GBufferDepth, uv).r;
    
    // Skip background
    if (centerDepth >= 1.0) {
        imageStore(u_ShadowOutput, pixelCoords, vec4(1.0));
        return;
    }
    
    // Bilateral filter
    float weightSum = 0.0;
    float shadowSum = 0.0;
    
    int radius = int(u_FilterRadius);
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            vec2 offset = vec2(x, y) * texelSize;
            vec2 sampleUV = uv + offset;
            
            // Sample neighbor
            float sampleShadow = texture(u_ShadowInput, sampleUV).r;
            vec3 sampleNormal = texture(u_GBufferNormal, sampleUV).rgb;
            float sampleDepth = texture(u_GBufferDepth, sampleUV).r;
            
            // Spatial weight (Gaussian)
            float spatialDist = length(vec2(x, y));
            float spatialWeight = exp(-spatialDist * spatialDist / (2.0 * u_FilterRadius * u_FilterRadius));
            
            // Normal weight (edge-aware)
            float normalSimilarity = max(0.0, dot(centerNormal, sampleNormal));
            float normalWeight = pow(normalSimilarity, 32.0);
            
            // Depth weight (edge-aware)
            float depthDiff = abs(centerDepth - sampleDepth);
            float depthWeight = exp(-depthDiff / u_DepthThreshold);
            
            // Combined weight
            float weight = spatialWeight * normalWeight * depthWeight;
            
            shadowSum += sampleShadow * weight;
            weightSum += weight;
        }
    }
    
    // Normalize
    float filteredShadow = shadowSum / max(weightSum, 0.0001);
    
    imageStore(u_ShadowOutput, pixelCoords, vec4(filteredShadow, filteredShadow, filteredShadow, 1.0));
}
