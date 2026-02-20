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
	vec2 dir = uv - vec2(0.5);
	float dist = length(dir);
	vec2 offset = dir * dist * intensity;

	float r = texture(u_SceneColor, uv + offset).r;
	float g = texture(u_SceneColor, uv).g;
	float b = texture(u_SceneColor, uv - offset).b;

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
