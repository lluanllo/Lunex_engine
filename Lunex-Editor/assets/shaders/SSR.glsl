#version 450 core

// ============================================================================
// SCREEN SPACE REFLECTIONS - Hierarchical Ray Marching with IBL Fallback
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

// ============ INPUT TEXTURES ============
layout(binding = 0) uniform sampler2D u_SceneColor;
layout(binding = 1) uniform sampler2D u_SceneDepth;
layout(binding = 2) uniform sampler2D u_SceneNormal;

// ============ IBL/SKYBOX FALLBACK ============
layout(binding = 3) uniform samplerCube u_EnvironmentMap;

// ============ SSR PARAMETERS ============
layout(std140, binding = 6) uniform SSRData {
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_InvProjectionMatrix;
    mat4 u_InvViewMatrix;
    vec2 u_ViewSize;
    float u_MaxDistance;
    float u_Resolution;
    float u_Thickness;
    float u_StepSize;
    int u_MaxSteps;
    float u_Intensity;
    float u_RoughnessThreshold;
    float u_EdgeFade;
    int u_Enabled;
    int u_UseEnvironmentFallback;
    float u_EnvironmentIntensity;
    float _padding;
};

// ============ CONSTANTS ============
const float EPSILON = 0.0001;
const float PI = 3.14159265359;

// ============ UTILITY FUNCTIONS ============

// Reconstruct view-space position from UV and depth
vec3 GetPositionVS(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_InvProjectionMatrix * clipPos;
    return viewPos.xyz / viewPos.w;
}

// Project view-space position to screen space [0,1]
vec3 ProjectToScreen(vec3 posVS) {
    vec4 clipPos = u_ProjectionMatrix * vec4(posVS, 1.0);
    if (clipPos.w <= 0.0) return vec3(-1.0); // Behind camera
    vec3 ndc = clipPos.xyz / clipPos.w;
    return vec3(ndc.xy * 0.5 + 0.5, ndc.z * 0.5 + 0.5);
}

// Linear depth from hyperbolic depth buffer
float LinearizeDepth(float depth) {
    float near = u_ProjectionMatrix[3][2] / (u_ProjectionMatrix[2][2] - 1.0);
    float far = u_ProjectionMatrix[3][2] / (u_ProjectionMatrix[2][2] + 1.0);
    return (2.0 * near * far) / (far + near - (depth * 2.0 - 1.0) * (far - near));
}

float GetEdgeFade(vec2 uv) {
    vec2 fade = smoothstep(0.0, u_EdgeFade, uv) * smoothstep(1.0, 1.0 - u_EdgeFade, uv);
    return fade.x * fade.y;
}

// ============ HIERARCHICAL RAY MARCH ============

vec4 RayMarchSSR(vec3 originVS, vec3 normalVS, vec3 reflectDirVS) {
    // Initial offset along normal to prevent self-intersection
    // This is critical - offset by a distance proportional to the view depth
    float originDepth = -originVS.z;
    float offsetScale = max(originDepth * 0.005, 0.02);
    vec3 startVS = originVS + normalVS * offsetScale;
    
    // Project start point
    vec3 startScreen = ProjectToScreen(startVS);
    if (startScreen.x < 0.0) return vec4(0.0); // Behind camera
    
    // Calculate end point in view space
    vec3 endVS = startVS + reflectDirVS * u_MaxDistance;
    vec3 endScreen = ProjectToScreen(endVS);
    
    // If end is behind camera, clip the ray
    if (endScreen.x < 0.0) {
        // Find intersection with near plane
        float t = (-startVS.z - 0.1) / reflectDirVS.z;
        if (t > 0.0) {
            endVS = startVS + reflectDirVS * min(t, u_MaxDistance);
            endScreen = ProjectToScreen(endVS);
        }
        if (endScreen.x < 0.0) return vec4(0.0);
    }
    
    // Ray in screen space
    vec3 rayScreen = endScreen - startScreen;
    
    // Calculate adaptive step count based on screen-space distance
    float screenDist = length(rayScreen.xy * u_ViewSize);
    
    // Use hierarchical stepping: start with large steps, refine on hit
    int coarseSteps = clamp(int(screenDist * u_Resolution * 0.25), 8, u_MaxSteps / 4);
    int fineSteps = 8; // Binary refinement steps
    
    vec3 coarseStep = rayScreen / float(coarseSteps);
    vec3 rayPos = startScreen;
    
    // Thickness based on distance - increased base value for better hit detection
    float baseThickness = u_Thickness * 0.05;  // Increased from 0.005
    
    // Coarse pass - find approximate intersection
    vec3 hitPos = vec3(0.0);
    bool foundHit = false;
    int hitStep = 0;
    
    for (int i = 1; i <= coarseSteps; i++) {
        rayPos += coarseStep;
        
        // Screen bounds check
        if (rayPos.x < 0.0 || rayPos.x > 1.0 || 
            rayPos.y < 0.0 || rayPos.y > 1.0) {
            break;
        }
        
        // Depth bounds (behind camera or too far)
        if (rayPos.z < 0.0 || rayPos.z > 1.0) {
            break;
        }
        
        // Sample scene depth
        float sceneDepth = texture(u_SceneDepth, rayPos.xy).r;
        
        // Skip skybox pixels
        if (sceneDepth > 0.9999) {
            continue;
        }
        
        // Check for hit: ray is behind scene surface
        float depthDiff = rayPos.z - sceneDepth;
        
        // Adaptive thickness increases with ray distance
        float thickness = baseThickness * (1.0 + float(i) * 0.1);
        
        if (depthDiff > 0.0 && depthDiff < thickness) {
            // Potential hit - verify it's valid geometry
            vec4 hitNormalData = texture(u_SceneNormal, rayPos.xy);
            
            // Check if we have valid normal data (packed normals are in 0-1 range)
            // A valid packed normal should have reasonable length
            vec3 unpackedNormal = hitNormalData.rgb * 2.0 - 1.0;
            float normalLen = length(unpackedNormal);
            
            // Skip invalid geometry (no normal data = not a valid surface)
            if (normalLen < 0.5) {
                continue;
            }
            
            // Check for back-face hit (avoid reflecting inside meshes)
            vec3 hitNormalWS = normalize(unpackedNormal);
            vec3 hitNormalVS = normalize(mat3(u_ViewMatrix) * hitNormalWS);
            
            // The hit surface's normal should face toward the ray origin
            // If it faces away, we're hitting the back of a surface
            // Use a more lenient threshold
            if (dot(hitNormalVS, -reflectDirVS) < -0.1) {
                continue; // Back-face, skip
            }
            
            hitPos = rayPos;
            foundHit = true;
            hitStep = i;
            break;
        }
    }
    
    if (!foundHit) {
        return vec4(0.0);
    }
    
    // Binary search refinement for precise hit location
    vec3 refinedPos = hitPos;
    vec3 refinedStep = coarseStep * 0.5;
    
    for (int j = 0; j < fineSteps; j++) {
        float refinedDepth = texture(u_SceneDepth, refinedPos.xy).r;
        float refinedDiff = refinedPos.z - refinedDepth;
        
        if (refinedDiff > 0.0) {
            refinedPos -= refinedStep;
        } else {
            refinedPos += refinedStep;
        }
        refinedStep *= 0.5;
    }
    
    // Clamp to screen bounds after refinement
    refinedPos.xy = clamp(refinedPos.xy, vec2(0.001), vec2(0.999));
    
    // Final validation - check we still have valid geometry
    vec4 finalNormalData = texture(u_SceneNormal, refinedPos.xy);
    vec3 finalUnpacked = finalNormalData.rgb * 2.0 - 1.0;
    if (length(finalUnpacked) < 0.5) {
        return vec4(0.0);
    }
    
    // Calculate fade factors
    float distFade = 1.0 - pow(float(hitStep) / float(coarseSteps), 1.5);
    float edgeFade = GetEdgeFade(refinedPos.xy);
    
    // Fresnel fade - reflections pointing toward camera are less accurate
    float fresnelFade = clamp(1.0 - abs(reflectDirVS.z), 0.2, 1.0);
    
    // Depth accuracy fade
    float finalDepth = texture(u_SceneDepth, refinedPos.xy).r;
    float depthAccuracy = 1.0 - smoothstep(0.0, baseThickness * 0.5, abs(refinedPos.z - finalDepth));
    
    float totalFade = distFade * edgeFade * fresnelFade * max(depthAccuracy, 0.3);
    
    // Sample color at hit point
    vec3 hitColor = texture(u_SceneColor, refinedPos.xy).rgb;
    
    return vec4(hitColor, totalFade);
}

// ============ FRESNEL FOR REFLECTIVITY ============

float FresnelSchlick(float cosTheta, float F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============ ENVIRONMENT MAP SAMPLING ============

vec3 SampleEnvironmentFallback(vec3 reflectDirWS, float roughness) {
    // Sample environment map with roughness-based LOD
    float lod = roughness * 4.0; // Assuming 4 mip levels
    vec3 envColor = textureLod(u_EnvironmentMap, reflectDirWS, lod).rgb;
    return envColor * u_EnvironmentIntensity;
}

// ============ MAIN ============

void main() {
    // Disabled check
    if (u_Enabled == 0) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Get depth first for early exit
    float depth = texture(u_SceneDepth, v_TexCoord).r;
    
    // Skip far background (skybox)
    if (depth > 0.9999) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Get normal and reflectivity data for the reflecting surface
    vec4 normalData = texture(u_SceneNormal, v_TexCoord);
    
    // Skip invalid geometry (no normal = not a renderable surface)
    // Packed normals are in [0,1], so a valid normal unpacked should have length ~1
    vec3 unpackedN = normalData.rgb * 2.0 - 1.0;
    float normalLength = length(unpackedN);
    if (normalLength < 0.5) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Reflectivity from G-buffer (combines metallic, specular, and smoothness)
    float reflectivity = normalData.a;
    
    // Skip surfaces with very low reflectivity
    if (reflectivity < 0.01) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Decode world-space normal from [0,1] to [-1,1]
    vec3 normalWS = normalize(unpackedN);
    
    // Reconstruct view-space position
    vec3 posVS = GetPositionVS(v_TexCoord, depth);
    
    // Transform world-space normal to view-space
    // Use the upper-left 3x3 of view matrix for direction transform
    vec3 normalVS = normalize(mat3(u_ViewMatrix) * normalWS);
    
    // View direction (from surface toward camera in view-space)
    vec3 viewDirVS = normalize(-posVS);
    
    // Reflection direction in view-space
    vec3 reflectDirVS = reflect(-viewDirVS, normalVS);
    
    // Calculate NdotV for Fresnel
    float NdotV = max(dot(normalVS, viewDirVS), 0.0);
    
    // Apply Fresnel to reflectivity - surfaces reflect more at grazing angles
    float fresnelReflectivity = FresnelSchlick(NdotV, reflectivity);
    
    // Reflection pointing toward camera won't find valid screen hits
    // Fade out based on how much the reflection goes into the screen
    float backFaceFade = smoothstep(-0.1, 0.4, reflectDirVS.z);
    
    // Perform ray marching
    vec4 ssrResult = RayMarchSSR(posVS, normalVS, reflectDirVS);
    
    // SSR contribution
    float ssrAlpha = ssrResult.a * fresnelReflectivity * backFaceFade;
    vec3 finalColor = ssrResult.rgb;
    float finalAlpha = ssrAlpha;
    
    // Environment fallback for missed rays
    if (u_UseEnvironmentFallback != 0 && ssrAlpha < 0.9) {
        // Calculate world-space reflection direction for cubemap sampling
        vec3 reflectDirWS = normalize(mat3(u_InvViewMatrix) * reflectDirVS);
        
        // Weight for environment: fill in where SSR missed
        // Higher for rays pointing away from screen (where SSR can't help)
        float envWeight = (1.0 - ssrAlpha) * fresnelReflectivity;
        
        // Boost environment for upward-facing reflections (sky)
        float skyBias = max(reflectDirWS.y, 0.0);
        envWeight *= (1.0 + skyBias * 0.5);
        
        if (envWeight > 0.005) {
            // Estimate roughness from reflectivity (inverse relationship)
            // Lower reflectivity often means rougher surface
            float estimatedRoughness = 1.0 - sqrt(reflectivity);
            
            vec3 envColor = SampleEnvironmentFallback(reflectDirWS, estimatedRoughness);
            
            // Blend: SSR where available, environment where not
            // Use max blending to avoid darkening
            float totalWeight = ssrAlpha + envWeight;
            if (totalWeight > EPSILON) {
                finalColor = (ssrResult.rgb * ssrAlpha + envColor * envWeight) / totalWeight;
                finalAlpha = min(totalWeight, 1.0);
            }
        }
    }
    
    FragColor = vec4(finalColor, finalAlpha);
}

#endif
