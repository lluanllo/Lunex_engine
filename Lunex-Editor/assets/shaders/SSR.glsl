#version 450 core

// ============================================================================
// SCREEN SPACE REFLECTIONS - Improved Linear Ray Marching
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

// ============ SSR PARAMETERS ============
layout(std140, binding = 6) uniform SSRData {
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_InvProjectionMatrix;
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
    float _padding;
};

// ============ CONSTANTS ============
const float EPSILON = 0.0001;

// ============ UTILITY FUNCTIONS ============

float LinearizeDepth(float depth) {
    float near = 0.1;
    float far = 1000.0;
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

vec3 GetPositionVS(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_InvProjectionMatrix * clipPos;
    return viewPos.xyz / viewPos.w;
}

vec3 ProjectToScreen(vec3 posVS) {
    vec4 clipPos = u_ProjectionMatrix * vec4(posVS, 1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    return vec3(ndc.xy * 0.5 + 0.5, ndc.z * 0.5 + 0.5);
}

float GetEdgeFade(vec2 uv) {
    vec2 fade = smoothstep(0.0, u_EdgeFade, uv) * smoothstep(1.0, 1.0 - u_EdgeFade, uv);
    return fade.x * fade.y;
}

// ============ SCREEN SPACE RAY MARCH ============

vec4 RayMarchSSR(vec3 originVS, vec3 dirVS, float roughness) {
    // Project start and end points to screen space
    vec3 startScreen = ProjectToScreen(originVS);
    vec3 endVS = originVS + dirVS * u_MaxDistance;
    vec3 endScreen = ProjectToScreen(endVS);
    
    // Ray direction in screen space
    vec3 rayScreen = endScreen - startScreen;
    
    // Calculate step count based on screen distance
    float screenDist = length(rayScreen.xy * u_ViewSize);
    int stepCount = clamp(int(screenDist * u_Resolution), 1, u_MaxSteps);
    vec3 stepScreen = rayScreen / float(stepCount);
    
    // Add jitter based on roughness to hide banding
    float jitter = fract(sin(dot(v_TexCoord, vec2(12.9898, 78.233))) * 43758.5453);
    vec3 rayPos = startScreen + stepScreen * jitter * (1.0 + roughness);
    
    float thickness = u_Thickness * 0.01;
    
    for (int i = 0; i < stepCount; i++) {
        rayPos += stepScreen;
        
        // Check screen bounds with margin
        if (rayPos.x < 0.01 || rayPos.x > 0.99 || 
            rayPos.y < 0.01 || rayPos.y > 0.99 ||
            rayPos.z < 0.0 || rayPos.z > 1.0) {
            break;
        }
        
        // Sample scene depth
        float sceneDepth = texture(u_SceneDepth, rayPos.xy).r;
        
        // Compare depths
        float depthDiff = rayPos.z - sceneDepth;
        
        // Hit detection: ray went behind surface
        if (depthDiff > 0.0 && depthDiff < thickness) {
            // Binary search refinement for precision
            vec3 refinedPos = rayPos;
            vec3 refinedStep = stepScreen * 0.5;
            
            for (int j = 0; j < 4; j++) {
                float refinedDepth = texture(u_SceneDepth, refinedPos.xy).r;
                float refinedDiff = refinedPos.z - refinedDepth;
                
                if (refinedDiff > 0.0) {
                    refinedPos -= refinedStep;
                } else {
                    refinedPos += refinedStep;
                }
                refinedStep *= 0.5;
            }
            
            // Calculate fade factors
            float distFade = 1.0 - (float(i) / float(stepCount));
            distFade = pow(distFade, 0.5); // Less aggressive distance fade
            
            float edgeFade = GetEdgeFade(refinedPos.xy);
            
            // Fresnel-like fade for grazing angles
            float fresnelFade = 1.0 - pow(abs(dirVS.z), 2.0);
            fresnelFade = clamp(fresnelFade, 0.3, 1.0);
            
            float totalFade = distFade * edgeFade * fresnelFade;
            
            // Sample color at refined hit point
            vec3 hitColor = texture(u_SceneColor, refinedPos.xy).rgb;
            
            return vec4(hitColor, totalFade);
        }
    }
    
    return vec4(0.0);
}

// ============ MAIN ============

void main() {
    // Disabled check
    if (u_Enabled == 0) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Get normal and reflectivity mask
    vec4 normalData = texture(u_SceneNormal, v_TexCoord);
    
    // Skip background (no normal data)
    float normalLength = length(normalData.rgb);
    if (normalLength < 0.1) {
        FragColor = vec4(0.0);
        return;
    }
    
    float reflectivity = normalData.a;
    
    // Skip non-reflective surfaces
    if (reflectivity < 0.05) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Decode normal from [0,1] to [-1,1]
    vec3 normalWS = normalize(normalData.rgb * 2.0 - 1.0);
    
    // Get depth
    float depth = texture(u_SceneDepth, v_TexCoord).r;
    
    // Skip far background
    if (depth > 0.9999) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Reconstruct view-space position
    vec3 posVS = GetPositionVS(v_TexCoord, depth);
    
    // Transform normal to view space
    vec3 normalVS = normalize(mat3(u_ViewMatrix) * normalWS);
    
    // View direction (from surface to camera, camera at origin in VS)
    vec3 viewDirVS = normalize(-posVS);
    
    // Reflection direction
    vec3 reflectDirVS = reflect(-viewDirVS, normalVS);
    
    // Fade out reflections pointing towards camera (won't find hits anyway)
    // Use smooth fade instead of hard cutoff to avoid line artifacts
    float backFaceFade = smoothstep(-0.1, 0.3, -reflectDirVS.z);
    
    if (backFaceFade < 0.01) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Estimate roughness (inverse of reflectivity)
    float roughness = clamp(1.0 - reflectivity, 0.0, 1.0);
    
    // Skip if too rough
    if (roughness > u_RoughnessThreshold) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Perform ray marching
    vec4 ssrResult = RayMarchSSR(posVS, reflectDirVS, roughness);
    
    // Apply all fade factors
    float roughnessFade = 1.0 - (roughness / u_RoughnessThreshold);
    roughnessFade = pow(roughnessFade, 0.5);
    
    float finalAlpha = ssrResult.a * reflectivity * backFaceFade * roughnessFade;
    
    FragColor = vec4(ssrResult.rgb, finalAlpha);
}

#endif
