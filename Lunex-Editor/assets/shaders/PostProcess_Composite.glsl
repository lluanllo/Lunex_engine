#version 450 core

/**
 * PostProcess - Final Composite
 * 
 * Combines: Scene Color + Bloom + Vignette + Chromatic Aberration
 * Then applies tone mapping and gamma correction.
 */

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoords;

layout(location = 0) out vec2 v_TexCoords;

void main() {
	v_TexCoords = a_TexCoords;
	gl_Position = vec4(a_Position, 1.0);
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 v_TexCoords;

// ============ SAMPLERS ============
layout(binding = 0) uniform sampler2D u_SceneColor;
layout(binding = 1) uniform sampler2D u_BloomTexture;

// ============ UNIFORMS (packed in UBO for SPIR-V) ============
layout(std140, binding = 0) uniform CompositeParams {
	// Bloom
	int   u_EnableBloom;
	float u_BloomIntensity;
	// Vignette
	int   u_EnableVignette;
	float u_VignetteIntensity;
	float u_VignetteRoundness;
	float u_VignetteSmoothness;
	// Chromatic Aberration
	int   u_EnableChromaticAberration;
	float u_ChromaticAberrationIntensity;
	// Tone Mapping
	int   u_ToneMapOperator;  // 0=ACES, 1=Reinhard, 2=Uncharted2, 3=None
	float u_Exposure;
	float u_Gamma;
	float _pad0;
};

// ============ TONE MAP FUNCTIONS ============

vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Reinhard(vec3 color) {
	return color / (color + vec3(1.0));
}

vec3 Uncharted2Partial(vec3 x) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 Uncharted2(vec3 color) {
	float W = 11.2;
	vec3 curr = Uncharted2Partial(color * 2.0);
	vec3 whiteScale = 1.0 / Uncharted2Partial(vec3(W));
	return curr * whiteScale;
}

// ============ CHROMATIC ABERRATION ============

vec3 SampleWithChromaticAberration(vec2 uv, float intensity) {
	// Barrel distortion-based chromatic aberration
	// Each color channel is sampled at a slightly different radial distance
	vec2 center = uv - 0.5;
	float dist2 = dot(center, center); // squared distance from center
	
	// Scale intensity so that even at max values the offset stays reasonable
	// dist2 at corners ? 0.5, so max offset per channel = 0.5 * intensity * 0.5 ? intensity * 0.02
	float scaledIntensity = intensity * 0.04;
	vec2 offset = center * (dist2 * scaledIntensity);
	
	// Red shifts outward, blue shifts inward, green stays centered
	vec2 uvR = uv + offset;
	vec2 uvB = uv - offset;
	
	// Fade out the effect when UVs approach the texture border
	// This prevents hard color fringe at screen edges
	vec2 fadeR = smoothstep(vec2(0.0), vec2(0.02), uvR) * smoothstep(vec2(0.0), vec2(0.02), vec2(1.0) - uvR);
	vec2 fadeB = smoothstep(vec2(0.0), vec2(0.02), uvB) * smoothstep(vec2(0.0), vec2(0.02), vec2(1.0) - uvB);
	float weightR = fadeR.x * fadeR.y;
	float weightB = fadeB.x * fadeB.y;
	
	// Clamp UVs to valid range
	uvR = clamp(uvR, vec2(0.001), vec2(0.999));
	uvB = clamp(uvB, vec2(0.001), vec2(0.999));
	
	// Sample with fallback: when near the edge, blend toward the centered (green) sample
	vec3 centerColor = texture(u_SceneColor, uv).rgb;
	float r = mix(centerColor.r, texture(u_SceneColor, uvR).r, weightR);
	float g = centerColor.g;
	float b = mix(centerColor.b, texture(u_SceneColor, uvB).b, weightB);

	return vec3(r, g, b);
}

// ============ VIGNETTE ============

float ComputeVignette(vec2 uv, float intensity, float roundness, float smoothness) {
	vec2 center = uv - 0.5;
	center.x *= roundness;
	float dist = length(center) * 2.0;
	float vignette = 1.0 - smoothstep(1.0 - smoothness, 1.0, dist * intensity);
	return vignette;
}

// ============ MAIN ============

void main() {
	vec3 color;

	// Sample scene color (with optional chromatic aberration)
	if (u_EnableChromaticAberration != 0) {
		color = SampleWithChromaticAberration(v_TexCoords, u_ChromaticAberrationIntensity);
	} else {
		color = texture(u_SceneColor, v_TexCoords).rgb;
	}

	// Add bloom
	if (u_EnableBloom != 0) {
		vec3 bloom = texture(u_BloomTexture, v_TexCoords).rgb;
		color += bloom * u_BloomIntensity;
	}

	// Exposure
	color *= u_Exposure;

	// Tone mapping
	if (u_ToneMapOperator == 0) {
		color = ACESFilm(color);
	} else if (u_ToneMapOperator == 1) {
		color = Reinhard(color);
	} else if (u_ToneMapOperator == 2) {
		color = Uncharted2(color);
	}
	// else: no tone mapping (u_ToneMapOperator == 3)

	// Gamma correction
	color = pow(color, vec3(1.0 / u_Gamma));

	// Vignette (after gamma for correct visual appearance)
	if (u_EnableVignette != 0) {
		float vignette = ComputeVignette(v_TexCoords, u_VignetteIntensity, u_VignetteRoundness, u_VignetteSmoothness);
		color *= vignette;
	}

	FragColor = vec4(color, 1.0);
}

#endif
