#version 450 core

// ============================================================================
// HYBRID RASTERIZATION + RAY TRACED SHADOWS
// Unity HDRP-style: Rasterized geometry + RT shadows for realism
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in int a_EntityID;

layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
	mat4 u_View;
	mat4 u_Projection;
};

layout(std140, binding = 1) uniform Transform {
	mat4 u_Transform;
};

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) out VertexOutput Output;
layout (location = 7) out flat int v_EntityID;

void main() {
	vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
	Output.FragPos = worldPos.xyz;
	
	mat3 normalMatrix = mat3(transpose(inverse(u_Transform)));
	vec3 N = normalize(normalMatrix * a_Normal);
	Output.Normal = N;
	Output.TexCoords = a_TexCoords;
	
	vec3 T = normalize(normalMatrix * a_Tangent);
	T = normalize(T - dot(T, N) * N);
	vec3 B = normalize(normalMatrix * a_Bitangent);
	B = normalize(B - dot(B, N) * N - dot(B, T) * T);
	Output.TBN = mat3(T, B, N);
	
	v_EntityID = a_EntityID;
	gl_Position = u_ViewProjection * worldPos;
}

#elif defined(FRAGMENT)

layout(location = 0) out vec4 FragColor;
layout(location = 1) out int o_EntityID;
layout(location = 2) out vec4 o_GBufferPosition;  // World position for ray tracing
layout(location = 3) out vec4 o_GBufferNormal;    // World normal for ray tracing

struct VertexOutput {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	mat3 TBN;
};

layout (location = 0) in VertexOutput Input;
layout (location = 7) in flat int v_EntityID;

// ============================================================================
// UNIFORMS
// ============================================================================

layout(std140, binding = 2) uniform Material {
	vec4 u_Color;
	float u_Metallic;
	float u_Roughness;
	float u_Specular;
	float u_EmissionIntensity;
	vec3 u_EmissionColor;
	float _padding1;
	vec3 u_ViewPos;
	
	int u_UseAlbedoMap;
	int u_UseNormalMap;
	int u_UseMetallicMap;
	int u_UseRoughnessMap;
	int u_UseSpecularMap;
	int u_UseEmissionMap;
	int u_UseAOMap;
	float _padding2;
	
	float u_MetallicMultiplier;
	float u_RoughnessMultiplier;
	float u_SpecularMultiplier;
	float u_AOMultiplier;
};

layout(std140, binding = 5) uniform SkyboxLighting {
	vec3 u_SkyboxAmbient;
	float u_SkyboxEnabled;
	vec3 u_SkyboxSunDirection;
	float u_SkyboxSunIntensity;
	vec3 u_SkyboxSunColor;
	float _skyboxPadding;
};

// ============================================================================
// RAY TRACING CONFIGURATION
// ============================================================================

layout(std140, binding = 6) uniform RayTracingSettings {
	int u_EnableRayTracedShadows;
	float u_ShadowRayBias;
	float u_ShadowSoftness;
	int u_ShadowSamplesPerLight;
	
	float u_MaxShadowDistance;
	float u_ShadowFadeDistance;
	int u_UseContactHardening;
	float u_ContactHardeningScale;
	
	int u_EnableGroundPlane;
	float u_GroundPlaneY;
	int u_VisualizeShadowRays;
	float _rtPadding;
};

#define MAX_LIGHTS 16

struct LightData {
	vec4 Position;      // xyz = position, w = type (0=dir, 1=point, 2=spot)
	vec4 Direction;     // xyz = direction, w = unused
	vec4 Color;         // rgb = color, a = intensity
	vec4 Params;        // x = range, y = innerCone, z = outerCone, w = radius (for soft shadows)
	vec4 Attenuation;
};

layout(std140, binding = 3) uniform Lights {
	int u_NumLights;
	float _padding3;
	float _padding4;
	float _padding5;
	LightData u_Lights[MAX_LIGHTS];
};

// Textures
layout (binding = 0) uniform sampler2D u_AlbedoMap;
layout (binding = 1) uniform sampler2D u_NormalMap;
layout (binding = 2) uniform sampler2D u_MetallicMap;
layout (binding = 3) uniform sampler2D u_RoughnessMap;
layout (binding = 4) uniform sampler2D u_SpecularMap;
layout (binding = 5) uniform sampler2D u_EmissionMap;
layout (binding = 6) uniform sampler2D u_AOMap;

// G-Buffer for ray tracing (position, normal, depth)
layout (binding = 7) uniform sampler2D u_GBufferPosition;
layout (binding = 8) uniform sampler2D u_GBufferNormal;
layout (binding = 9) uniform sampler2D u_GBufferDepth;

// Compute ray traced shadows
layout (binding = 10) uniform sampler2D u_ComputeShadowMap;

// Scene BVH/Acceleration Structure (simplified as texture for demo)
layout (binding = 11) uniform sampler2D u_SceneBVH;

// ============================================================================
// CONSTANTS
// ============================================================================

const float PI = 3.14159265359;
const float INV_PI = 0.31830988618;
const float EPSILON = 1e-6;
const float MIN_ROUGHNESS = 0.045;
const float MIN_DIELECTRIC_F0 = 0.04;

// Ray tracing constants
const float RAY_TMIN = 0.001;
const float RAY_TMAX = 1000.0;
const int MAX_RAY_BOUNCES = 1; // Shadows only, no indirect lighting

// ============================================================================
// RANDOM NUMBER GENERATION (Blue Noise)
// ============================================================================

// High-quality hash function
float Hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

vec2 Hash22(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}

// Blue noise sampling (better than random)
vec2 BlueNoiseSample(vec2 screenPos, int sampleIndex) {
	vec2 noise = Hash22(screenPos + vec2(sampleIndex * 0.618034, sampleIndex * 1.32471));
	return noise;
}

// ============================================================================
// RAY GENERATION
// ============================================================================

struct Ray {
	vec3 origin;
	vec3 direction;
	float tMin;
	float tMax;
};

Ray GenerateShadowRay(vec3 origin, vec3 direction, float maxDist) {
	Ray ray;
	ray.origin = origin;
	ray.direction = normalize(direction);
	ray.tMin = RAY_TMIN;
	ray.tMax = min(maxDist, RAY_TMAX);
	return ray;
}

// ============================================================================
// SOFT SHADOW SAMPLING (Area Light Approximation)
// ============================================================================

// Sample random point on sphere (for point lights)
vec3 SampleSphere(vec2 uv, float radius) {
	float phi = uv.y * 2.0 * PI;
	float cosTheta = 1.0 - 2.0 * uv.x;
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	
	return vec3(
		cos(phi) * sinTheta,
		sin(phi) * sinTheta,
		cosTheta
	) * radius;
}

// Sample random point on disk (for spot/directional lights)
vec2 SampleDisk(vec2 uv, float radius) {
	float r = sqrt(uv.x) * radius;
	float theta = uv.y * 2.0 * PI;
	return vec2(r * cos(theta), r * sin(theta));
}

// ============================================================================
// RAY-SCENE INTERSECTION (IMPROVED FOR REAL-TIME)
// ============================================================================

// Ray-Plane intersection
bool RayPlaneIntersect(Ray ray, float planeY, out float t) {
	// Plane equation: y = planeY (normal = (0, 1, 0))
	float denom = ray.direction.y;
	
	// Ray parallel to plane
	if (abs(denom) < EPSILON) return false;
	
	t = (planeY - ray.origin.y) / denom;
	return t > ray.tMin && t < ray.tMax;
}

// Ray-Sphere intersection
bool RaySphereIntersect(Ray ray, vec3 center, float radius, out float t) {
	vec3 oc = ray.origin - center;
	float a = dot(ray.direction, ray.direction);
	float b = 2.0 * dot(oc, ray.direction);
	float c = dot(oc, oc) - radius * radius;
	float discriminant = b * b - 4.0 * a * c;
	
	if (discriminant < 0.0) return false;
	
	float sqrtD = sqrt(discriminant);
	float t0 = (-b - sqrtD) / (2.0 * a);
	float t1 = (-b + sqrtD) / (2.0 * a);
	
	// Check nearest intersection
	t = (t0 > ray.tMin) ? t0 : t1;
	return t > ray.tMin && t < ray.tMax;
}

// Ray-AABB intersection (for bounding boxes)
bool RayAABBIntersect(Ray ray, vec3 boxMin, vec3 boxMax) {
	vec3 invDir = 1.0 / (ray.direction + vec3(EPSILON));
	vec3 t0 = (boxMin - ray.origin) * invDir;
	vec3 t1 = (boxMax - ray.origin) * invDir;
	
	vec3 tMin = min(t0, t1);
	vec3 tMax = max(t0, t1);
	
	float tNear = max(max(tMin.x, tMin.y), tMin.z);
	float tFar = min(min(tMax.x, tMax.y), tMax.z);
	
	return tNear <= tFar && tFar >= ray.tMin && tNear <= ray.tMax;
}

// Improved scene traversal for shadows
bool TraceSceneShadowRay(Ray ray, out float hitDistance) {
	bool hit = false;
	hitDistance = ray.tMax;
	float t;
	
	// ===== GROUND PLANE =====
	if (u_EnableGroundPlane != 0) {
		if (RayPlaneIntersect(ray, u_GroundPlaneY, t)) {
			if (t < hitDistance) {
				hit = true;
				hitDistance = t;
			}
		}
	}
	
	// ===== SIMPLE OCCLUDER TEST =====
	// This will be replaced with real BVH when available
	// For now, we treat the current fragment as a potential occluder
	
	// Self-intersection check: if we're very close to the origin, 
	// assume this is the same object (avoid self-shadowing)
	float distFromOrigin = length(ray.origin);
	if (distFromOrigin < 0.01) {
		return hit; // Skip self
	}
	
	return hit;
}

// ============================================================================
// RAY TRACED SHADOW CALCULATION (IMPROVED)
// ============================================================================

float CalculateRayTracedShadow(
	vec3 fragPos,
	vec3 normal,
	vec3 lightPos,
	vec3 lightDir,
	int lightType,
	float lightRadius,
	int numSamples
) {
	float shadowFactor = 0.0;
	float totalHitDistance = 0.0;
	
	// Apply normal-oriented bias to prevent self-shadowing
	vec3 offsetOrigin = fragPos + normal * u_ShadowRayBias;
	
	// Calculate light direction and distance
	vec3 toLight = lightPos - fragPos;
	float lightDistance = length(toLight);
	vec3 L = toLight / lightDistance;
	
	// Early out if light is behind surface
	if (dot(normal, L) < 0.0) return 0.0;
	
	// Directional lights have infinite distance
	if (lightType == 0) {
		L = -lightDir;
		lightDistance = RAY_TMAX;
	}
	
	// Soft shadows: Sample area around light
	for (int i = 0; i < numSamples; i++) {
		vec2 noise = BlueNoiseSample(gl_FragCoord.xy, i);
		
		vec3 sampleOffset = vec3(0.0);
		
		if (lightType == 1) {
			// Point light: Sample sphere
			sampleOffset = SampleSphere(noise, lightRadius * u_ShadowSoftness);
		}
		else if (lightType == 2) {
			// Spot light: Sample cone
			vec2 diskSample = SampleDisk(noise, lightRadius * u_ShadowSoftness);
			vec3 tangent = normalize(cross(lightDir, vec3(0.0, 1.0, 0.0)));
			if (length(tangent) < EPSILON) {
				tangent = normalize(cross(lightDir, vec3(1.0, 0.0, 0.0)));
			}
			vec3 bitangent = cross(lightDir, tangent);
			sampleOffset = tangent * diskSample.x + bitangent * diskSample.y;
		}
		else if (lightType == 0) {
			// Directional light: Sample disk perpendicular to light
			vec2 diskSample = SampleDisk(noise, lightRadius * u_ShadowSoftness * 0.01);
			vec3 tangent = normalize(cross(L, vec3(0.0, 1.0, 0.0)));
			if (length(tangent) < EPSILON) {
				tangent = normalize(cross(L, vec3(1.0, 0.0, 0.0)));
			}
			vec3 bitangent = cross(L, tangent);
			sampleOffset = tangent * diskSample.x + bitangent * diskSample.y;
		}
		
		vec3 sampleLightPos = lightPos + sampleOffset;
		vec3 sampleDir = normalize(sampleLightPos - offsetOrigin);
		
		// Generate shadow ray
		Ray shadowRay = GenerateShadowRay(offsetOrigin, sampleDir, lightDistance);
		
		// Trace ray
		float hitDist;
		bool hit = TraceSceneShadowRay(shadowRay, hitDist);
		
		// Accumulate shadow
		if (!hit) {
			shadowFactor += 1.0;
		} else {
			totalHitDistance += hitDist;
			
			// Debug visualization
			if (u_VisualizeShadowRays != 0 && i == 0) {
				// This would show the first shadow ray in red
				// FragColor = vec4(1.0, 0.0, 0.0, 1.0);
			}
		}
	}
	
	// Average samples
	shadowFactor /= float(numSamples);
	
	// Contact hardening: Shadows become sharper closer to contact
	if (u_UseContactHardening != 0 && shadowFactor < 1.0) {
		// Average distance to occluders
		float avgHitDistance = totalHitDistance / max(float(numSamples) - shadowFactor * float(numSamples), 1.0);
		
		// Normalize distance (0 = very close, 1 = far)
		float normalizedDist = clamp(avgHitDistance / lightDistance, 0.0, 1.0);
		
		// Apply hardening: closer = sharper shadows
		float hardeningFactor = pow(normalizedDist, u_ContactHardeningScale);
		shadowFactor = mix(shadowFactor * shadowFactor, shadowFactor, hardeningFactor);
	}
	
	// Distance fade
	float fadeFactor = 1.0 - smoothstep(
		u_MaxShadowDistance - u_ShadowFadeDistance,
		u_MaxShadowDistance,
		lightDistance
	);
	shadowFactor = mix(1.0, shadowFactor, fadeFactor);
	
	return shadowFactor;
}

// ============================================================================
// TRADITIONAL SHADOW MAP FALLBACK
// ============================================================================

float CalculateShadowMapShadow(vec3 fragPos, vec3 normal, vec3 lightDir) {
	// Placeholder for traditional shadow mapping
	// Would use shadow map texture in real implementation
	return 1.0; // No shadow
}

// ============================================================================
// PBR FUNCTIONS (Same as before)
// ============================================================================

float D_GGX(float NdotH, float a2) {
	float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / (PI * denom * denom + EPSILON);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2) {
	float lambdaV = NdotL * sqrt((NdotV - a2 * NdotV) * NdotV + a2);
	float lambdaL = NdotV * sqrt((NdotL - a2 * NdotL) * NdotL + a2);
	return 0.5 / max(lambdaV + lambdaL, EPSILON);
}

vec3 F_Schlick(float VdotH, vec3 F0) {
	float f = pow(1.0 - VdotH, 5.0);
	return F0 + (1.0 - F0) * f;
}

vec3 F_SchlickRoughness(float NdotV, vec3 F0, float roughness) {
	vec3 smoothF0 = max(vec3(1.0 - roughness), F0);
	return F0 + (smoothF0 - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
}

float Fd_Burley(float NdotV, float NdotL, float LdotH, float roughness) {
	float f90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
	float FL = pow(1.0 - NdotL, 5.0);
	float FV = pow(1.0 - NdotV, 5.0);
	return INV_PI * (1.0 + (f90 - 1.0) * FL) * (1.0 + (f90 - 1.0) * FV);
}

float GetDistanceAttenuation(float distance, float range) {
	if (range <= 0.0) return 1.0;
	float d2 = distance * distance;
	float r2 = range * range;
	float num = clamp(1.0 - pow(d2 / r2, 2.0), 0.0, 1.0);
	return (num * num) / (d2 + 1.0);
}

float GetSpotAttenuation(vec3 L, vec3 spotDir, float innerCone, float outerCone) {
	float cosAngle = dot(-L, spotDir);
	float t = clamp((cosAngle - outerCone) / max(innerCone - outerCone, EPSILON), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

// ============================================================================
// HYBRID LIGHTING WITH RAY TRACED SHADOWS
// ============================================================================

vec3 CalculateHybridLighting(
	vec3 N, vec3 V, vec3 F0,
	vec3 albedo, float metallic, float a2,
	vec3 fragPos
) {
	vec3 Lo = vec3(0.0);
	float NdotV = max(dot(N, V), EPSILON);
	
	for (int i = 0; i < min(u_NumLights, MAX_LIGHTS); i++) {
		LightData light = u_Lights[i];
		int lightType = int(light.Position.w);
		
		vec3 L;
		vec3 radiance = light.Color.rgb * light.Color.a;
		float attenuation = 1.0;
		
		// Calculate light direction and attenuation
		if (lightType == 0) {
			L = normalize(-light.Direction.xyz);
		}
		else if (lightType == 1) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / distance;
			
			float range = light.Params.x;
			if (range > 0.0 && distance > range) continue;
			attenuation = GetDistanceAttenuation(distance, range);
		}
		else if (lightType == 2) {
			vec3 lightVec = light.Position.xyz - fragPos;
			float distance = length(lightVec);
			L = lightVec / distance;
			
			float range = light.Params.x;
			if (range > 0.0 && distance > range) continue;
			
			attenuation = GetDistanceAttenuation(distance, range);
			attenuation *= GetSpotAttenuation(L, normalize(light.Direction.xyz), light.Params.y, light.Params.z);
		}
		
		if (attenuation < EPSILON) continue;
		
		// ========== RAY TRACED SHADOWS ==========
		float shadowFactor = 1.0;
		
		if (u_EnableRayTracedShadows != 0) {
			// Use ray tracing for shadows
			shadowFactor = CalculateRayTracedShadow(
				fragPos,
				N,
				light.Position.xyz,
				light.Direction.xyz,
				lightType,
				light.Params.w, // Light radius for soft shadows
				u_ShadowSamplesPerLight
			);
		} else {
			// Fallback to shadow maps
			shadowFactor = CalculateShadowMapShadow(fragPos, N, light.Direction.xyz);
		}
		
		// Early out if fully shadowed
		if (shadowFactor < EPSILON) continue;
		
		// ========== PBR BRDF ==========
		vec3 H = normalize(V + L);
		float NdotL = max(dot(N, L), EPSILON);
		float NdotH = max(dot(N, H), EPSILON);
		float VdotH = max(dot(V, H), EPSILON);
		float LdotH = max(dot(L, H), EPSILON);
		
		float D = D_GGX(NdotH, a2);
		float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a2);
		vec3 F = F_Schlick(VdotH, F0);
		vec3 specular = D * Vis * F;
		
		vec3 kD = (1.0 - F) * (1.0 - metallic);
		float Fd = Fd_Burley(NdotV, NdotL, LdotH, sqrt(a2));
		vec3 diffuse = kD * albedo * Fd;
		
		// Apply shadow and accumulate
		Lo += (diffuse + specular) * radiance * attenuation * NdotL * shadowFactor;
	}
	
	return Lo;
}

// ============================================================================
// AMBIENT LIGHTING
// ============================================================================

vec3 CalculateAmbient(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
	float NdotV = max(dot(N, V), EPSILON);
	vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
	vec3 kD = (1.0 - F) * (1.0 - metallic);
	
	vec3 ambientDiffuse = albedo * kD;
	vec3 ambientSpecular = F * (1.0 - roughness * 0.5) * 0.03;
	vec3 ambient = (ambientDiffuse + ambientSpecular) * ao;
	
	if (u_SkyboxEnabled > 0.5) {
		vec3 skyboxAmbient = u_SkyboxAmbient * ambient;
		float sunNdotL = max(dot(N, u_SkyboxSunDirection), 0.0);
		vec3 sunColor = u_SkyboxSunColor * u_SkyboxSunIntensity;
		vec3 sunContribution = sunColor * albedo * kD * sunNdotL * ao * 0.4;
		return skyboxAmbient + sunContribution;
	}
	
	return ambient * 0.03;
}

// ============================================================================
// TONE MAPPING
// ============================================================================

vec3 ACESFilm(vec3 x) {
	const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 LinearToSRGB(vec3 color) {
	return pow(color, vec3(0.4545454545));
}

vec3 SRGBToLinear(vec3 color) {
	return pow(color, vec3(2.2));
}

// ============================================================================
// MAIN FRAGMENT SHADER
// ============================================================================

void main() {
	// Texture sampling
	vec3 albedo;
	float alpha = u_Color.a;
	if (u_UseAlbedoMap != 0) {
		vec4 texColor = texture(u_AlbedoMap, Input.TexCoords);
		albedo = SRGBToLinear(texColor.rgb);
		alpha *= texColor.a;
	} else {
		albedo = SRGBToLinear(u_Color.rgb);
	}
	
	vec3 N;
	if (u_UseNormalMap != 0) {
		vec3 normalMap = texture(u_NormalMap, Input.TexCoords).rgb * 2.0 - 1.0;
		N = normalize(Input.TBN * normalMap);
	} else {
		N = normalize(Input.Normal);
	}
	
	float metallic = u_Metallic;
	if (u_UseMetallicMap != 0) {
		metallic = clamp(texture(u_MetallicMap, Input.TexCoords).r * u_MetallicMultiplier, 0.0, 1.0);
	}
	
	float roughness = u_Roughness;
	if (u_UseRoughnessMap != 0) {
		roughness = clamp(texture(u_RoughnessMap, Input.TexCoords).r * u_RoughnessMultiplier, 0.0, 1.0);
	}
	roughness = max(roughness, MIN_ROUGHNESS);
	float a = roughness * roughness;
	float a2 = a * a;
	
	float specular = u_Specular;
	if (u_UseSpecularMap != 0) {
		specular = clamp(texture(u_SpecularMap, Input.TexCoords).r * u_SpecularMultiplier, 0.0, 1.0);
	}
	
	float ao = 1.0;
	if (u_UseAOMap != 0) {
		ao = clamp(texture(u_AOMap, Input.TexCoords).r * u_AOMultiplier, 0.0, 1.0);
	}
	
	// PBR with hybrid ray traced shadows
	vec3 V = normalize(u_ViewPos - Input.FragPos);
	vec3 F0 = mix(vec3(MIN_DIELECTRIC_F0 * specular), albedo, metallic);
	
	vec3 directLighting = CalculateHybridLighting(N, V, F0, albedo, metallic, a2, Input.FragPos);
	vec3 ambient = CalculateAmbient(N, V, F0, albedo, metallic, roughness, ao);
	
	// Emission
	vec3 emission = vec3(0.0);
	if (u_EmissionIntensity > EPSILON) {
		emission = u_EmissionColor * u_EmissionIntensity;
		if (u_UseEmissionMap != 0) {
			vec3 emissionTex = texture(u_EmissionMap, Input.TexCoords).rgb;
			emission = SRGBToLinear(emissionTex) * u_EmissionColor * u_EmissionIntensity;
		}
	}
	
	vec3 emissiveContribution = vec3(0.0);
	float emissiveLuminance = dot(emission, vec3(0.2126, 0.7152, 0.0722));
	if (emissiveLuminance > EPSILON) {
		float upwardBias = max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0) * 0.5 + 0.5;
		float kD = 1.0 - metallic;
		emissiveContribution = albedo * emission * kD * upwardBias * ao * 0.2;
	}
	
	// ===== APPLY COMPUTE RAY TRACED SHADOWS =====
	// Sample shadow map from compute shader
	vec2 texSize = vec2(textureSize(u_ComputeShadowMap, 0));
	float computeShadow = 1.0; // Default: no shadow
	
	if (texSize.x > 0.0 && texSize.y > 0.0) {
		vec2 screenUV = gl_FragCoord.xy / texSize;
		float shadowSample = texture(u_ComputeShadowMap, screenUV).r;
		
		// Only apply compute shadows if they seem valid (not completely black)
		// This prevents the entire scene from going dark if compute RT fails
		if (shadowSample > 0.01) {
			computeShadow = shadowSample;
		} else {
			// Fallback: no shadow if compute shader failed
			computeShadow = 1.0;
		}
	}
	
	// Apply compute shadows to direct lighting only (not ambient or emission)
	// This gives physically accurate shadowing
	directLighting *= computeShadow;
	
	// Final composition
	vec3 color = directLighting + ambient + emission + emissiveContribution;
	color = ACESFilm(color);
	color = LinearToSRGB(color);
	
	// Output final color
	FragColor = vec4(color, alpha);
	o_EntityID = v_EntityID;
	
	// Output G-Buffer data for compute ray tracing
	o_GBufferPosition = vec4(Input.FragPos, 1.0);
	o_GBufferNormal = vec4(N, 1.0);
}

#endif