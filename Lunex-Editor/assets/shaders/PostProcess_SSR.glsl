#version 450 core

/**
 * PostProcess - Screen Space Reflections
 *
 * View-space ray march against the G-Buffer position buffer, refined with a
 * binary search. Outputs the reflection already weighted by Fresnel, roughness
 * and a confidence term (screen edge / distance / facing fades), so the composite
 * pass only has to add it to the scene color.
 *
 * Inputs:
 *   u_SceneColor        (0): lit HDR scene (copy, not the render target)
 *   gNormalRoughness    (1): world normal (encoded 0..1) + roughness
 *   gAlbedoMetallic     (2): albedo + metallic
 *   gPositionSpecular   (3): world position + specular
 *   gDepth              (4): hardware depth (1.0 = sky)
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

layout(binding = 0) uniform sampler2D u_SceneColor;
layout(binding = 1) uniform sampler2D gNormalRoughness;
layout(binding = 2) uniform sampler2D gAlbedoMetallic;
layout(binding = 3) uniform sampler2D gPositionSpecular;
layout(binding = 4) uniform sampler2D gDepth;

layout(std140, binding = 0) uniform SSRParams {
	mat4  u_View;
	mat4  u_Projection;
	float u_MaxDistance;     // max ray length in world units
	float u_Thickness;       // depth tolerance for a hit (world units)
	int   u_MaxSteps;
	float u_MaxRoughness;    // surfaces rougher than this get no SSR
	float u_Intensity;
	float u_EdgeFade;        // fraction of the screen used to fade out at borders
	float _pad0;
	float _pad1;
};

const int BINARY_STEPS = 6;

float InterleavedGradientNoise(vec2 pixel) {
	return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

vec3 ViewPosAt(vec2 uv) {
	vec3 world = texture(gPositionSpecular, uv).xyz;
	return (u_View * vec4(world, 1.0)).xyz;
}

vec2 ProjectToUV(vec3 viewPos) {
	vec4 clip = u_Projection * vec4(viewPos, 1.0);
	return (clip.xy / clip.w) * 0.5 + 0.5;
}

void main() {
	float depth = texture(gDepth, v_TexCoords).r;
	if (depth >= 1.0) { FragColor = vec4(0.0); return; }

	vec4 normalRough = texture(gNormalRoughness, v_TexCoords);
	float roughness = normalRough.a;
	if (roughness > u_MaxRoughness) { FragColor = vec4(0.0); return; }

	vec4 albedoMetal = texture(gAlbedoMetallic, v_TexCoords);
	vec4 posSpec = texture(gPositionSpecular, v_TexCoords);

	vec3 worldN = normalize(normalRough.rgb * 2.0 - 1.0);
	vec3 viewPos = (u_View * vec4(posSpec.xyz, 1.0)).xyz;
	vec3 viewN = normalize(mat3(u_View) * worldN);
	vec3 V = normalize(-viewPos);
	vec3 R = normalize(reflect(-V, viewN));

	// Rays pointing back at the camera rarely hit anything visible on screen
	float facingFade = 1.0 - smoothstep(0.25, 0.6, R.z);
	if (facingFade <= 0.0) { FragColor = vec4(0.0); return; }

	// ---- Linear march with jittered start ----
	float stepLen = u_MaxDistance / float(max(u_MaxSteps, 1));
	float jitter = InterleavedGradientNoise(gl_FragCoord.xy);
	vec3 origin = viewPos + viewN * 0.02; // avoid self-intersection

	vec3 prevP = origin;
	vec3 hitP = vec3(0.0);
	bool hit = false;
	float traveled = 0.0;

	for (int i = 1; i <= u_MaxSteps; i++) {
		traveled = (float(i) - 1.0 + jitter) * stepLen;
		vec3 p = origin + R * traveled;
		vec2 uv = ProjectToUV(p);
		if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;
		if (p.z > -0.01) break; // behind the camera

		if (texture(gDepth, uv).r >= 1.0) { prevP = p; continue; } // sky: keep going

		float sceneZ = ViewPosAt(uv).z;
		float delta = sceneZ - p.z; // > 0 when the ray is behind the surface
		if (delta > 0.0 && delta < u_Thickness + stepLen) {
			hit = true;
			hitP = p;
			break;
		}
		prevP = p;
	}

	if (!hit) { FragColor = vec4(0.0); return; }

	// ---- Binary refinement between last miss and first hit ----
	vec3 a = prevP;
	vec3 b = hitP;
	for (int i = 0; i < BINARY_STEPS; i++) {
		vec3 mid = (a + b) * 0.5;
		float sceneZ = ViewPosAt(ProjectToUV(mid)).z;
		if (sceneZ - mid.z > 0.0) b = mid; else a = mid;
	}
	vec2 hitUV = ProjectToUV(b);

	// Verify the refined hit with the real thickness. The coarse test above accepts up to
	// thickness + stepLen, which lets rays passing *behind* an object count as hits
	// (stretched reflections).
	float hitDelta = ViewPosAt(hitUV).z - b.z;
	if (hitDelta > u_Thickness) { FragColor = vec4(0.0); return; }
	// Marginal hits (close to the thickness limit) fade out instead of popping on/off
	float hitFade = 1.0 - smoothstep(u_Thickness * 0.35, u_Thickness, hitDelta);

	// Reject hits on back-facing surfaces (reflection would show the wrong side)
	vec3 hitN = normalize(mat3(u_View) * (texture(gNormalRoughness, hitUV).rgb * 2.0 - 1.0));
	if (dot(hitN, R) > 0.0) { FragColor = vec4(0.0); return; }

	// ---- Confidence ----
	vec2 edge = smoothstep(vec2(0.0), vec2(u_EdgeFade), hitUV) *
	            smoothstep(vec2(0.0), vec2(u_EdgeFade), vec2(1.0) - hitUV);
	float edgeFade = edge.x * edge.y;
	float distanceFade = 1.0 - smoothstep(0.6, 1.0, traveled / u_MaxDistance);
	float roughFade = 1.0 - smoothstep(u_MaxRoughness * 0.5, u_MaxRoughness, roughness);
	float confidence = edgeFade * distanceFade * roughFade * facingFade * hitFade;

	// ---- Specular weight (Schlick Fresnel with the same F0 as the lighting pass) ----
	float metallic = albedoMetal.a;
	vec3 F0 = mix(vec3(0.04 * posSpec.a * 2.0), albedoMetal.rgb, metallic);
	float NdotV = max(dot(viewN, V), 0.0);
	vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
	float glossy = (1.0 - roughness) * (1.0 - roughness);

	vec3 reflected = texture(u_SceneColor, hitUV).rgb;
	FragColor = vec4(reflected * F * glossy * confidence * u_Intensity, confidence);
}

#endif
