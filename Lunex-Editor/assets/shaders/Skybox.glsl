#version 450 core

// ============================================================================
// ADVANCED PROCEDURAL SKYBOX SHADER - ULTRA REALISTIC
// Fixed: Stars rendering, cubemap faces, smooth transitions
// ============================================================================

#ifdef VERTEX

layout(location = 0) in vec3 a_Position;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_ViewDir;

layout(std140, binding = 0) uniform Camera {
	mat4 u_ViewProjection;
	mat4 u_View;
	mat4 u_Projection;
};

void main() {
	v_WorldPos = a_Position;
	v_ViewDir = normalize(a_Position);
	
	mat4 viewNoTranslation = mat4(mat3(u_View));
	vec4 pos = u_Projection * viewNoTranslation * vec4(a_Position, 1.0);
	gl_Position = pos.xyww;
}

#elif defined(FRAGMENT)

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_ViewDir;

layout(location = 0) out vec4 FragColor;

// ============================================================================
// UNIFORMS
// ============================================================================

layout(std140, binding = 4) uniform SkyboxParams {
	vec3 u_SunDirection;
	float u_TimeOfDay;
	vec3 u_SunColor;
	float u_SunIntensity;
	vec3 u_GroundAlbedo;
	float u_Turbidity;
	float u_RayleighCoefficient;
	float u_MieCoefficient;
	float u_MieDirectionalG;
	float u_Exposure;
	float u_UseCubemap;
	float u_SkyTint;
	float u_AtmosphereThickness;
	float u_GroundEmission;
	float u_HDRExposure;
	float u_SkyboxRotation;
};

layout (binding = 0) uniform samplerCube u_CubemapTexture;

// ============================================================================
// CONSTANTS
// ============================================================================

const float PI = 3.14159265359;
const float EARTH_RADIUS = 6371e3;
const float ATMOSPHERE_RADIUS = 6471e3;
const vec3 BETA_R = vec3(5.5e-6, 13.0e-6, 22.4e-6);
const vec3 BETA_M = vec3(21e-6);
const float H_R = 7994.0;
const float H_M = 1200.0;

// ============================================================================
// HASH FUNCTIONS - SMOOTH NOISE
// ============================================================================

float Hash11(float p) {
	p = fract(p * 0.1031);
	p *= p + 33.33;
	p *= p + p;
	return fract(p);
}

float Hash13(vec3 p3) {
	p3 = fract(p3 * 0.1031);
	p3 += dot(p3, p3.zyx + 31.32);
	return fract((p3.x + p3.y) * p3.z);
}

vec2 Hash23(vec3 p3) {
	p3 = fract(p3 * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}

// ============================================================================
// ATMOSPHERIC SCATTERING
// ============================================================================

float PhaseRayleigh(float cosTheta) {
	return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

float PhaseMie(float cosTheta, float g) {
	float g2 = g * g;
	float denom = 1.0 + g2 - 2.0 * g * cosTheta;
	return (1.0 / (4.0 * PI)) * ((1.0 - g2) / pow(denom, 1.5));
}

float RaySphereIntersect(vec3 rayOrigin, vec3 rayDir, float sphereRadius) {
	float a = dot(rayDir, rayDir);
	float b = 2.0 * dot(rayOrigin, rayDir);
	float c = dot(rayOrigin, rayOrigin) - sphereRadius * sphereRadius;
	float discriminant = b * b - 4.0 * a * c;
	if (discriminant < 0.0) return -1.0;
	return (-b + sqrt(discriminant)) / (2.0 * a);
}

vec3 ComputeAtmosphericScattering(vec3 rayDir, vec3 sunDir, vec3 rayOrigin) {
	float viewElevation = rayDir.y;
	if (viewElevation < -0.15) return vec3(0.0);
	
	float tAtmosphere = RaySphereIntersect(rayOrigin, rayDir, ATMOSPHERE_RADIUS);
	if (tAtmosphere < 0.0) return vec3(0.0);
	
	float zenithAngle = acos(clamp(viewElevation, 0.0, 1.0));
	float airMass = 1.0 / (cos(zenithAngle) + 0.15 * pow(max(93.885 - degrees(zenithAngle), 0.0), -1.253));
	airMass = clamp(airMass, 1.0, 40.0);
	
	vec3 betaR = BETA_R * u_RayleighCoefficient;
	vec3 betaM = BETA_M * u_MieCoefficient;
	
	float cosTheta = dot(rayDir, sunDir);
	float phaseR = PhaseRayleigh(cosTheta);
	float phaseM = PhaseMie(cosTheta, u_MieDirectionalG);
	
	vec3 extinction = exp(-(betaR + betaM) * airMass);
	extinction = smoothstep(vec3(0.0), vec3(1.0), extinction);
	
	vec3 scatteringR = betaR * phaseR;
	vec3 scatteringM = betaM * phaseM * 0.08;
	vec3 inscatter = (scatteringR + scatteringM) * (1.0 - extinction) * 18.0;
	
	float zenithBoost = mix(1.0, 2.8, pow(max(0.0, viewElevation), 0.35));
	float horizonFade = smoothstep(-0.15, 0.25, viewElevation);
	
	return inscatter * zenithBoost * horizonFade;
}

// ============================================================================
// SUN RENDERING - ULTRA REALISTIC
// ============================================================================

vec3 RenderSun(vec3 rayDir, vec3 sunDir) {
	float sunDot = dot(rayDir, sunDir);
	float sunAngularRadius = 0.0093 * 3.0; // Más grande
	
	// Disco solar con gradiente suave
	float sunDisk = smoothstep(cos(sunAngularRadius * 1.3), cos(sunAngularRadius * 0.7), sunDot);
	
	// Centro más brillante
	float centerGlow = pow(max(0.0, sunDot), 512.0) * 2.0;
	
	// Coronas múltiples
	float corona1 = pow(max(0.0, sunDot), 256.0) * 1.5;
	float corona2 = pow(max(0.0, sunDot), 128.0) * 0.8;
	float corona3 = pow(max(0.0, sunDot), 64.0) * 0.4;
	float corona4 = pow(max(0.0, sunDot), 32.0) * 0.2;
	
	// Halo atmosférico
	float halo = pow(max(0.0, sunDot), 16.0) * 0.3;
	
	vec3 sunColor = u_SunColor * 1.3;
	vec3 sunContribution = sunColor * (
		sunDisk * 25.0 +
		centerGlow + corona1 + corona2 + corona3 + corona4 + halo
	);
	
	float visibility = smoothstep(-0.2, 0.05, sunDir.y);
	return sunContribution * visibility;
}

// ============================================================================
// MOON RENDERING - ULTRA REALISTIC
// ============================================================================

vec3 RenderMoon(vec3 rayDir, vec3 sunDir) {
	vec3 moonDir = -sunDir;
	float nightBlend = smoothstep(0.1, -0.3, sunDir.y);
	if (nightBlend < 0.01) return vec3(0.0);
	
	float moonDot = dot(rayDir, moonDir);
	float moonAngularRadius = 0.0093 * 3.0; // Más grande
	
	// Disco lunar suave
	float moonDisk = smoothstep(cos(moonAngularRadius * 1.2), cos(moonAngularRadius * 0.85), moonDot);
	
	// Fases lunares
	float phase = dot(moonDir, vec3(1.0, 0.0, 0.0)) * 0.5 + 0.5;
	phase = pow(phase, 0.7) * 0.6 + 0.4;
	
	// Textura lunar (cráteres)
	vec3 moonPos = rayDir * 200.0;
	float craters = Hash13(floor(moonPos * 8.0)) * 0.4;
	
	vec3 moonColor = vec3(0.88, 0.90, 0.94) * phase;
	moonColor *= (1.0 - craters * 0.3);
	
	// Halo lunar
	float moonGlow = pow(max(0.0, moonDot), 128.0) * 0.4;
	
	return (moonColor * moonDisk * 1.5 + vec3(0.7, 0.75, 0.85) * moonGlow) * nightBlend;
}

// ============================================================================
// STARS - COMPLETAMENTE ARREGLADAS (SIN CUADRADOS)
// ============================================================================

vec3 RenderStars(vec3 rayDir, float nightBlend) {
	if (nightBlend < 0.15 || rayDir.y < -0.02) return vec3(0.0);
	
	vec3 stars = vec3(0.0);
	vec3 dir = normalize(rayDir);
	
	// CAPA 1: Estrellas grandes y brillantes (ANTI-ALIASED)
	for(int layer = 0; layer < 3; layer++) {
		float scale = 150.0 + float(layer) * 50.0;
		vec3 p = dir * scale;
		vec3 cellId = floor(p);
		
		for(int dx = -1; dx <= 1; dx++) {
			for(int dy = -1; dy <= 1; dy++) {
				for(int dz = -1; dz <= 1; dz++) {
					vec3 offset = vec3(dx, dy, dz);
					vec3 cell = cellId + offset;
					
					float h = Hash13(cell);
					if(h > 0.998) {
						// Posición de la estrella dentro de la celda
						vec2 uv = Hash23(cell);
						vec3 starLocalPos = cell + vec3(uv.x, uv.y, Hash11(h));
						vec3 starDir = normalize(starLocalPos);
						
						// Distancia angular (NO euclidiana)
						float angularDist = acos(clamp(dot(dir, starDir), -1.0, 1.0));
						
						// Punto GAUSSIANO suave (NO cuadrado)
						float starSize = 0.002 + Hash11(h * 3.0) * 0.001;
						float star = exp(-angularDist * angularDist / (starSize * starSize));
						
						// Brillo variable
						float brightness = pow(h, 8.0) * (1.5 + sin(u_TimeOfDay * 2.0 + h * 100.0) * 0.3);
						
						// Color de estrella
						vec3 starColor = mix(
							vec3(1.0, 0.9, 0.8),
							vec3(0.8, 0.9, 1.0),
							Hash11(h * 7.0)
						);
						
						stars += starColor * star * brightness * 1.2;
					}
				}
			}
		}
	}
	
	// CAPA 2: Estrellas medianas
	float scale2 = 100.0;
	vec3 p2 = dir * scale2;
	vec3 cellId2 = floor(p2);
	
	for(int dx = -1; dx <= 1; dx++) {
		for(int dy = -1; dy <= 1; dy++) {
			for(int dz = -1; dz <= 1; dz++) {
				vec3 cell = cellId2 + vec3(dx, dy, dz);
				float h = Hash13(cell);
				
				if(h > 0.996) {
					vec2 uv = Hash23(cell);
					vec3 starDir = normalize(cell + vec3(uv, Hash11(h)));
					float angularDist = acos(clamp(dot(dir, starDir), -1.0, 1.0));
					float star = exp(-angularDist * angularDist / 0.000004);
					stars += vec3(0.9, 0.95, 1.0) * star * pow(h, 6.0) * 0.8;
				}
			}
		}
	}
	
	// Vía Láctea (niebla de estrellas)
	float theta = atan(dir.z, dir.x);
	float phi = asin(clamp(dir.y, -1.0, 1.0));
	float milkyWay = pow(max(0.0, sin(theta * 1.3 + 0.5)), 5.0) * pow(max(0.0, cos(phi * 2.5)), 4.0);
	float milkyNoise = Hash13(floor(dir * 50.0)) * 0.5 + 0.5;
	stars += vec3(0.4, 0.5, 0.7) * milkyWay * milkyNoise * 0.3;
	
	// Fade cerca del horizonte
	float horizonFade = smoothstep(-0.02, 0.15, rayDir.y);
	
	return stars * nightBlend * horizonFade * 2.5;
}

// ============================================================================
// GROUND - TRANSICIÓN ULTRA SUAVE
// ============================================================================

vec3 GroundColor(vec3 rayDir, vec3 sunDir, float nightBlend) {
	if (rayDir.y >= -0.01) return vec3(0.0);
	
	float groundDepth = -rayDir.y;
	float dayFactor = 1.0 - nightBlend;
	
	// Color base
	vec3 groundCol = u_GroundAlbedo * u_GroundEmission;
	
	// Iluminación ambiente
	vec3 ambientLight = mix(vec3(0.015, 0.02, 0.04), vec3(0.35, 0.4, 0.45), dayFactor);
	groundCol *= ambientLight;
	
	// Resplandor del horizonte ULTRA suave
	float horizonGlow = exp(-groundDepth * 12.0);
	
	vec3 glowColor;
	if (dayFactor > 0.5) {
		glowColor = vec3(0.5, 0.6, 0.75) * 0.25;
	} else {
		float sunsetFactor = smoothstep(0.15, -0.25, sunDir.y);
		glowColor = mix(vec3(0.25, 0.3, 0.4), vec3(0.85, 0.45, 0.25), sunsetFactor) * 0.2;
	}
	
	groundCol += glowColor * horizonGlow;
	
	// Transición SÚPER suave
	float blendFactor = smoothstep(-0.01, -0.4, rayDir.y);
	blendFactor = blendFactor * blendFactor; // Curva cuadrática para más suavidad
	
	return groundCol * blendFactor;
}

// ============================================================================
// TONE MAPPING
// ============================================================================

vec3 ACESFilm(vec3 x) {
	float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ============================================================================
// MAIN
// ============================================================================

void main() {
	vec3 rayDir = normalize(v_ViewDir);
	
	// ========== CUBEMAP MODE ==========
	if (u_UseCubemap > 0.5) {
		float cosRot = cos(u_SkyboxRotation);
		float sinRot = sin(u_SkyboxRotation);
		mat3 rotationMatrix = mat3(
			cosRot, 0.0, -sinRot,
			0.0, 1.0, 0.0,
			sinRot, 0.0, cosRot
		);
		vec3 rotatedDir = rotationMatrix * rayDir;
		
		vec3 color = texture(u_CubemapTexture, rotatedDir).rgb;
		color *= u_HDRExposure;
		color = ACESFilm(color);
		color = pow(color, vec3(1.0 / 2.2));
		
		FragColor = vec4(color, 1.0);
		return;
	}
	
	// ========== PROCEDURAL MODE ==========
	vec3 skyColor = vec3(0.0);
	vec3 rayOrigin = vec3(0.0, EARTH_RADIUS + 10.0, 0.0);
	
	float sunElevation = u_SunDirection.y;
	float nightBlend = smoothstep(0.15, -0.3, sunElevation);
	float dayBlend = 1.0 - nightBlend;
	
	// ========== ATMOSPHERIC SCATTERING ==========
	if (dayBlend > 0.01 && rayDir.y >= -0.15) {
		vec3 scattering = ComputeAtmosphericScattering(rayDir, u_SunDirection, rayOrigin);
		skyColor += scattering * u_SunColor * u_SunIntensity * dayBlend * u_AtmosphereThickness;
		
		float zenithBoost = pow(max(0.0, rayDir.y), 0.25) * 0.65;
		skyColor += vec3(0.2, 0.4, 0.8) * zenithBoost * dayBlend;
	}
	
	// ========== NIGHT SKY ==========
	if (nightBlend > 0.01 && rayDir.y >= -0.02) {
		float heightGradient = pow(max(0.0, rayDir.y), 0.35);
		vec3 nightSky = mix(vec3(0.015, 0.025, 0.06), vec3(0.001, 0.002, 0.012), heightGradient);
		skyColor += nightSky * nightBlend;
	}
	
	// ========== HORIZON ==========
	if (rayDir.y > 0.0 && rayDir.y < 0.4) {
		float horizonGradient = pow(1.0 - (rayDir.y / 0.4), 2.0);
		float sunsetFactor = smoothstep(0.3, -0.2, sunElevation);
		vec3 horizonColor = mix(vec3(0.4, 0.5, 0.7), vec3(0.95, 0.4, 0.2), sunsetFactor);
		skyColor += horizonColor * horizonGradient * dayBlend * 0.35;
	}
	
	// ========== SUN ==========
	skyColor += RenderSun(rayDir, u_SunDirection);
	
	// ========== MOON ==========
	skyColor += RenderMoon(rayDir, u_SunDirection);
	
	// ========== STARS (FIXED!) ==========
	skyColor += RenderStars(rayDir, nightBlend);
	
	// ========== GROUND (SMOOTH!) ==========
	skyColor += GroundColor(rayDir, u_SunDirection, nightBlend);
	
	// ========== POST-PROCESSING ==========
	skyColor *= u_Exposure;
	skyColor = ACESFilm(skyColor);
	skyColor = pow(skyColor, vec3(1.0 / 2.2));
	
	FragColor = vec4(skyColor, 1.0);
}

#endif