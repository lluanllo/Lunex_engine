#include "stpch.h"
#include "VulkanSceneRenderer.h"

#include "RHI/Vulkan/VulkanContext.h"
#include "RHI/Vulkan/VulkanTexture.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/Camera/CameraData.h"
#include "Resources/Mesh/Model.h"
#include "Resources/Render/MaterialInstance.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Assets/Materials/MaterialGraphCompiler.h"
#include "Graph/NodeGraph.h"
#include "Renderer/EnvironmentMap.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Texture.h"
#include "Renderer/Renderer2DVertex.h"
#include "Renderer/PostProcess/PostProcessRenderer.h"
#include "Log/Log.h"

#include <shaderc/shaderc.hpp>
#include <stb_image.h>
#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <unordered_map>

namespace Lunex {

	using namespace RHI::Vk;

	namespace {

		// ============================================================================
		// SHADERS (GLSL 450, compiled to SPIR-V with shaderc at startup / on first use)
		// ============================================================================

		const char* kCommon = R"GLSL(
#version 450
layout(set = 0, binding = 0) uniform FrameData {
	mat4 View; mat4 Proj; mat4 ViewProj; mat4 InvViewProj;
	vec4 CamPos;
	vec4 EnvParams;    // x intensity, y rotation, z has env, w max mip
	ivec4 Counts;      // x lights, z sun light index (-1 = none)
	mat4 LightViewProj;
	vec4 ShadowParams; // x enabled, y texel size (uv), z strength, w texel size (world)
	vec4 OutlineColor;
	vec4 Viewport;     // w, h, 1/w, 1/h
	vec4 Time;         // x seconds
} F;
struct Light { vec4 PosRange; vec4 DirType; vec4 Color; vec4 Cone; };
layout(set = 0, binding = 1, std430) readonly buffer Lights { Light L[]; };
layout(set = 0, binding = 2, std430) readonly buffer Bones { mat4 B[]; };
struct Material { vec4 Albedo; vec4 Emission; vec4 Params; ivec4 Maps; ivec4 Maps2; vec4 UV; vec4 Subsurface; vec4 Extra; };
layout(set = 0, binding = 3, std430) readonly buffer Materials { Material M[]; };
layout(set = 0, binding = 4) uniform sampler2D uEnv;
layout(set = 0, binding = 5) uniform sampler2DShadow uShadow;
layout(set = 0, binding = 6) uniform sampler2D uSceneColor;   // opaque scene (mipmapped) for refraction
layout(push_constant) uniform Push { mat4 Model; int Material; int Entity; int BoneOffset; int Skinned; } P;
const float PI = 3.14159265359;
)GLSL";

		const char* kMeshVS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
#ifdef SKINNED
layout(location = 6) in ivec4 aBones;
layout(location = 7) in vec4 aWeights;
#endif
layout(location = 0) out vec3 vPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vTangent;
layout(location = 4) out vec3 vBitangent;
void main() {
	mat4 model = P.Model;
#ifdef SKINNED
	mat4 skin = aWeights.x * B[P.BoneOffset + aBones.x] + aWeights.y * B[P.BoneOffset + aBones.y] +
	            aWeights.z * B[P.BoneOffset + aBones.z] + aWeights.w * B[P.BoneOffset + aBones.w];
	if (aWeights.x + aWeights.y + aWeights.z + aWeights.w > 0.0) model = model * skin;
#endif
	vec4 world = model * vec4(aPos, 1.0);
	mat3 nm = transpose(inverse(mat3(model)));
	vPos = world.xyz;
	vNormal = normalize(nm * aNormal);
	vTangent = normalize(nm * aTangent);
	vBitangent = normalize(nm * aBitangent);
	vUV = aUV;
	gl_Position = F.ViewProj * world;
}
)GLSL";

		// Shared surface lighting: GGX + Lambert, sun shadow, HDRI IBL, clear coat, sheen, subsurface
		const char* kShading = R"GLSL(
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;
layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntity;

float D_GGX(float NdotH, float r) { float a = r * r; float a2 = a * a; float d = NdotH * NdotH * (a2 - 1.0) + 1.0; return a2 / (PI * d * d); }
float V_Smith(float NdotV, float NdotL, float r) { float a = r * r; float gv = NdotL * sqrt(NdotV * NdotV * (1.0 - a * a) + a * a); float gl = NdotV * sqrt(NdotL * NdotL * (1.0 - a * a) + a * a); return 0.5 / max(gv + gl, 1e-5); }
vec3 F_Schlick(float c, vec3 f0) { return f0 + (1.0 - f0) * pow(1.0 - c, 5.0); }
float D_Charlie(float r, float NdotH) { float inv = 1.0 / max(r, 0.05); float s2 = 1.0 - NdotH * NdotH; return (2.0 + inv) * pow(max(s2, 0.0), inv * 0.5) / (2.0 * PI); }
float V_Neubelt(float NdotV, float NdotL) { return 1.0 / (4.0 * (NdotL + NdotV - NdotL * NdotV) + 1e-4); }
float Channel(vec4 s, int c) { return c == 0 ? s.r : c == 1 ? s.g : c == 2 ? s.b : s.a; }

vec2 EnvUV(vec3 d) {
	float c = cos(F.EnvParams.y), s = sin(F.EnvParams.y);
	d = vec3(c * d.x - s * d.z, d.y, s * d.x + c * d.z);
	return vec2(atan(d.z, d.x) / (2.0 * PI) + 0.5, acos(clamp(d.y, -1.0, 1.0)) / PI);
}
vec3 Env(vec3 d, float lod) {
	if (F.EnvParams.z < 0.5) return mix(vec3(0.25, 0.22, 0.2), vec3(0.45, 0.6, 0.85), clamp(d.y * 0.5 + 0.5, 0.0, 1.0)) * 0.6;
	return textureLod(uEnv, EnvUV(d), lod).rgb * F.EnvParams.x;
}

// Sun shadow: 5x5 PCF with hardware comparison (bilinear taps), normal offset against acne
float SunShadow(vec3 P, vec3 N, vec3 Ld) {
	float texel = F.ShadowParams.y;
	vec3 p = P + N * F.ShadowParams.w * (1.5 - dot(N, Ld));
	vec4 lp = F.LightViewProj * vec4(p, 1.0);
	vec3 s = lp.xyz / lp.w;
	vec2 uv = s.xy * 0.5 + 0.5;
	float depth = s.z * 0.5 + 0.5;
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || depth >= 1.0) return 1.0;
	float sum = 0.0;
	for (int y = -2; y <= 2; y++)
		for (int x = -2; x <= 2; x++)
			sum += texture(uShadow, vec3(uv + vec2(x, y) * texel, depth));
	float lit = sum / 25.0;
	vec2 edge = min(uv, 1.0 - uv);
	lit = mix(1.0, lit, clamp(min(edge.x, edge.y) * 20.0, 0.0, 1.0));
	return mix(1.0, lit, F.ShadowParams.z);
}

// Extra lobes (set by graph materials before calling Shade)
float gCoat = 0.0, gCoatRough = 0.03; vec3 gCoatTint = vec3(1.0);
float gSheen = 0.0, gSheenRough = 0.5; vec3 gSheenTint = vec3(1.0);
vec4 gSubsurface = vec4(0.0);
float gSunShadow = 1.0;

vec3 Shade(vec3 P, vec3 N, vec3 V, vec3 albedo, vec3 F0, float metallic, float roughness, float ao) {
	float NdotV = max(dot(N, V), 1e-4);
	vec3 Lo = vec3(0.0);
	gSunShadow = 1.0;
	for (int i = 0; i < F.Counts.x; i++) {
		Light l = L[i];
		int type = int(l.DirType.w);
		vec3 Ld;
		float att = 1.0;
		if (type == 0) Ld = normalize(-l.DirType.xyz);
		else {
			vec3 d = l.PosRange.xyz - P;
			float dist = length(d);
			Ld = d / max(dist, 1e-4);
			float range = max(l.PosRange.w, 1e-3);
			float x = dist / range;
			att = clamp(1.0 - x * x * x * x, 0.0, 1.0);
			att = att * att / (dist * dist + 1.0);
			if (type == 2) {
				float cd = dot(-Ld, normalize(l.DirType.xyz));
				att *= clamp((cd - l.Cone.y) / max(l.Cone.x - l.Cone.y, 1e-4), 0.0, 1.0);
			}
		}
		if (att <= 0.0) continue;
		float NdotLraw = dot(N, Ld);
		float shadow = 1.0;
		if (i == F.Counts.z && F.ShadowParams.x > 0.5) { shadow = SunShadow(P, N, Ld); gSunShadow = shadow; }
		vec3 radiance = l.Color.rgb * l.Color.w * att * shadow;
		// Subsurface: wrapped diffuse through the surface
		if (gSubsurface.a > 0.0) {
			float wrap = max((NdotLraw + 0.5) / 1.5, 0.0) - max(NdotLraw, 0.0);
			Lo += albedo * gSubsurface.rgb * gSubsurface.a * max(wrap, 0.0) * radiance / PI;
		}
		float NdotL = max(NdotLraw, 0.0);
		if (NdotL <= 0.0) continue;
		vec3 H = normalize(V + Ld);
		float NdotH = max(dot(N, H), 0.0), VdotH = max(dot(V, H), 0.0);
		vec3 Fs = F_Schlick(VdotH, F0);
		vec3 spec = D_GGX(NdotH, roughness) * V_Smith(NdotV, NdotL, roughness) * Fs;
		vec3 kd = (1.0 - Fs) * (1.0 - metallic);
		vec3 base = kd * albedo / PI + spec;
		if (gSheen > 0.0) base += gSheen * gSheenTint * D_Charlie(gSheenRough, NdotH) * V_Neubelt(NdotV, NdotL);
		if (gCoat > 0.0) {
			float Fc = F_Schlick(VdotH, vec3(0.04)).x * gCoat;
			float cr = max(gCoatRough, 0.03);
			base = base * gCoatTint * (1.0 - Fc) + vec3(D_GGX(NdotH, cr) * V_Smith(NdotV, NdotL, cr) * Fc);
		}
		Lo += base * radiance * NdotL;
	}
	// Image based lighting from the HDRI (mip by roughness); darker inside the sun shadow
	vec3 Fa = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
	vec3 diffuseIBL = Env(N, F.EnvParams.w - 1.0) * albedo * (1.0 - Fa) * (1.0 - metallic);
	vec3 specIBL = Env(reflect(-V, N), roughness * F.EnvParams.w) * Fa;
	vec3 ambient = (diffuseIBL + specIBL) * ao;
	if (gSheen > 0.0) ambient += gSheen * gSheenTint * Env(N, F.EnvParams.w - 1.0) * 0.15 * ao;
	if (gCoat > 0.0) {
		float Fc = F_Schlick(NdotV, vec3(0.04)).x * gCoat;
		ambient = ambient * gCoatTint * (1.0 - Fc) + Env(reflect(-V, N), max(gCoatRough, 0.03) * F.EnvParams.w) * Fc * ao;
	}
	if (F.Counts.z >= 0) ambient *= mix(0.35, 1.0, gSunShadow);
	return Lo + ambient;
}
)GLSL";

		// Classic PBR material (maps + parameters from the material asset)
		const char* kMeshFS = R"GLSL(
layout(set = 1, binding = 0) uniform sampler2D tAlbedo;
layout(set = 1, binding = 1) uniform sampler2D tNormal;
layout(set = 1, binding = 2) uniform sampler2D tMetallic;
layout(set = 1, binding = 3) uniform sampler2D tRoughness;
layout(set = 1, binding = 4) uniform sampler2D tAO;
layout(set = 1, binding = 5) uniform sampler2D tEmission;
layout(set = 1, binding = 6) uniform sampler2D tLayered;

void main() {
	Material m = M[P.Material];
	vec2 uv = vUV * m.UV.xy + m.UV.zw;
	vec4 albedo = m.Albedo;
	if (m.Maps.x != 0) { vec4 t = texture(tAlbedo, uv); albedo.rgb *= pow(max(t.rgb, vec3(0.0)), vec3(2.2)); albedo.a *= t.a; }
	if (albedo.a < 0.5) discard;

	vec3 N = normalize(vNormal);
	if (m.Maps.y != 0) {
		vec3 n = texture(tNormal, uv).rgb * 2.0 - 1.0;
		n.xy *= m.Params.z;
		N = normalize(mat3(normalize(vTangent), normalize(vBitangent), N) * normalize(n));
	}
	if (!gl_FrontFacing) N = -N;

	float metallic = m.Params.x, roughness = m.Params.y, ao = 1.0;
	vec4 layered = m.Maps2.z != 0 ? texture(tLayered, uv) : vec4(0.0);
	if (m.Maps2.z != 0 && (m.Maps2.w & 1) != 0) metallic = Channel(layered, (m.Maps2.w >> 4) & 3) * m.Subsurface.w;
	else if (m.Maps.z != 0) metallic = texture(tMetallic, uv).r * m.Subsurface.w;
	if (m.Maps2.z != 0 && (m.Maps2.w & 2) != 0) roughness = Channel(layered, (m.Maps2.w >> 6) & 3) * m.Emission.w;
	else if (m.Maps.w != 0) roughness = texture(tRoughness, uv).r * m.Emission.w;
	if (m.Maps2.z != 0 && (m.Maps2.w & 4) != 0) ao = Channel(layered, (m.Maps2.w >> 8) & 3);
	else if (m.Maps2.x != 0) ao = texture(tAO, uv).r;
	metallic = clamp(metallic, 0.0, 1.0);
	roughness = clamp(roughness, 0.045, 1.0);
	gSubsurface = vec4(m.Subsurface.rgb, m.Extra.x);

	vec3 V = normalize(F.CamPos.xyz - vPos);
	vec3 F0 = mix(vec3(0.08 * m.Params.w), albedo.rgb, metallic);
	vec3 color = Shade(vPos, N, V, albedo.rgb, F0, metallic, roughness, ao);
	vec3 emission = m.Emission.rgb;
	if (m.Maps2.y != 0) emission *= pow(texture(tEmission, uv).rgb, vec3(2.2));
	oColor = vec4(color + emission, 1.0);
	oEntity = P.Entity;
}
)GLSL";

		// Node graph material: generated code (MaterialGraphCompiler) between header and main
		const char* kGraphHeader = R"GLSL(
#define LNX_UV vUV
#define LNX_POS vPos
#define LNX_NORMAL ((gl_FrontFacing ? 1.0 : -1.0) * normalize(vNormal))
#define LNX_TBN mat3(normalize(vTangent), normalize(vBitangent), LNX_NORMAL)
#define LNX_VIEWPOS F.CamPos.xyz
#define LNX_VIEW F.View
#define LNX_VIEWPROJ F.ViewProj
#define LNX_MODEL P.Model
#define LNX_MODEL_INV inverse(P.Model)
#define LNX_OBJECT_ID float(P.Entity)
#define LNX_OBJPOS (LNX_MODEL_INV * vec4(LNX_POS, 1.0)).xyz
layout(set = 2, binding = 0, std430) readonly buffer GraphParams { vec4 u_GraphParams[]; };
layout(set = 1, binding = 0) uniform sampler2D u_GraphTex0;
layout(set = 1, binding = 1) uniform sampler2D u_GraphTex1;
layout(set = 1, binding = 2) uniform sampler2D u_GraphTex2;
layout(set = 1, binding = 3) uniform sampler2D u_GraphTex3;
layout(set = 1, binding = 4) uniform sampler2D u_GraphTex4;
layout(set = 1, binding = 5) uniform sampler2D u_GraphTex5;
layout(set = 1, binding = 6) uniform sampler2D u_GraphTex6;
layout(set = 1, binding = 7) uniform sampler2D u_GraphTex7;
layout(set = 1, binding = 8) uniform sampler2D u_GraphTex8;
layout(set = 1, binding = 9) uniform sampler2D u_GraphTex9;
layout(set = 1, binding = 10) uniform sampler2D u_GraphTex10;
layout(set = 1, binding = 11) uniform sampler2D u_GraphTex11;

// Shader to RGB: full lighting of a closure
vec3 lnx_light(vec3 albedo, float metallic, float roughness, float specular, vec3 N, float ao) {
	vec3 V = normalize(F.CamPos.xyz - vPos);
	roughness = max(roughness, 0.045);
	vec3 F0 = mix(vec3(0.08 * specular), albedo, metallic);
	return Shade(vPos, N, V, albedo, F0, metallic, roughness, ao);
}
)GLSL";

		const char* kGraphMain = R"GLSL(
void main() {
	LnxBSDF gm = lnx_evaluate_graph();
#ifndef FORWARD
	if (gm.Alpha < 0.5) discard;
#endif
	float metallic = clamp(gm.Metallic, 0.0, 1.0);
	float transmission = clamp(gm.Transmission, 0.0, 1.0) * (1.0 - metallic);
	vec3 baseColor = max(gm.BaseColor, vec3(0.0));
	vec3 albedo = baseColor * (1.0 - transmission);
	float alpha = clamp(gm.Alpha, 0.0, 1.0);
	vec3 N = normalize(gm.Normal);
	float roughness = max(clamp(gm.Roughness, 0.0, 1.0), 0.045);
	float specular = max(gm.Specular, 0.0);
	float ao = clamp(gm.AO, 0.0, 1.0);
	gSubsurface = gm.Subsurface;
	gCoat = clamp(gm.Coat, 0.0, 1.0); gCoatRough = gm.CoatRoughness; gCoatTint = gm.CoatTint;
	gSheen = clamp(gm.Sheen, 0.0, 1.0); gSheenRough = gm.SheenRoughness; gSheenTint = gm.SheenTint;

	vec3 V = normalize(F.CamPos.xyz - vPos);
	vec3 F0 = mix(vec3(0.08 * specular) * gm.SpecularTint, baseColor, metallic);
	vec3 color = Shade(vPos, N, V, albedo, F0, metallic, roughness, ao) + max(gm.Emission, vec3(0.0));
#ifdef FORWARD
	// Transmission: the opaque scene seen through the surface (refracted), blurred by roughness
	if (transmission > 0.0) {
		vec3 dir = gm.Refract > 0.5 ? refract(-V, N, 1.0 / max(gm.IOR, 1.0001)) : -V;
		if (dot(dir, dir) < 1e-6) dir = reflect(-V, N);
		vec3 Fv = F_Schlick(max(dot(N, V), 0.0), F0);
		vec4 clip = F.ViewProj * vec4(vPos + dir * 0.35, 1.0);
		vec2 uv = clamp(clip.xy / max(clip.w, 1e-4) * 0.5 + 0.5, vec2(0.001), vec2(0.999));
		vec3 behind = textureLod(uSceneColor, uv, roughness * 6.0).rgb;
		color += behind * baseColor * transmission * (1.0 - Fv);
	}
	oColor = vec4(color * alpha, alpha);   // premultiplied (blend ONE, ONE_MINUS_SRC_ALPHA)
#else
	oColor = vec4(color, 1.0);
#endif
	oEntity = P.Entity;
}
)GLSL";

		// Fullscreen triangle
		const char* kFullscreenVS = R"GLSL(
layout(location = 0) out vec2 vNDC;
void main() {
	vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vNDC = p * 2.0 - 1.0;
	gl_Position = vec4(vNDC, 1.0, 1.0);
}
)GLSL";

		const char* kSkyFS = R"GLSL(
layout(location = 0) in vec2 vNDC;
layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntity;
void main() {
	vec4 a = F.InvViewProj * vec4(vNDC, -1.0, 1.0);
	vec4 b = F.InvViewProj * vec4(vNDC, 1.0, 1.0);
	vec3 d = normalize(b.xyz / b.w - a.xyz / a.w);
	vec3 c;
	if (F.EnvParams.z > 0.5) {
		float cr = cos(F.EnvParams.y), sr = sin(F.EnvParams.y);
		vec3 r = vec3(cr * d.x - sr * d.z, d.y, sr * d.x + cr * d.z);
		vec2 uv = vec2(atan(r.z, r.x) / (2.0 * PI) + 0.5, acos(clamp(r.y, -1.0, 1.0)) / PI);
		c = textureLod(uEnv, uv, 0.0).rgb * F.EnvParams.x;
	}
	else c = mix(vec3(0.25, 0.22, 0.2), vec3(0.45, 0.6, 0.85), clamp(d.y * 0.5 + 0.5, 0.0, 1.0));
	oColor = vec4(c, 1.0);
	oEntity = -1;
}
)GLSL";

		const char* kGridFS = R"GLSL(
layout(location = 0) in vec2 vNDC;
layout(location = 0) out vec4 oColor;
void main() {
	vec4 a = F.InvViewProj * vec4(vNDC, -1.0, 1.0);
	vec4 b = F.InvViewProj * vec4(vNDC, 1.0, 1.0);
	vec3 o = a.xyz / a.w, e = b.xyz / b.w;
	vec3 d = e - o;
	if (abs(d.y) < 1e-6) discard;
	float t = -o.y / d.y;
	if (t <= 0.0 || t > 1.0) discard;
	vec3 p = o + d * t;
	vec2 g = p.xz;
	vec2 w = fwidth(g);
	vec2 lines = abs(fract(g - 0.5) - 0.5) / max(w, 1e-5);
	float minor = 1.0 - min(min(lines.x, lines.y), 1.0);
	vec2 g10 = g / 10.0;
	vec2 w10 = fwidth(g10);
	vec2 lines10 = abs(fract(g10 - 0.5) - 0.5) / max(w10, 1e-5);
	float major = 1.0 - min(min(lines10.x, lines10.y), 1.0);
	float alpha = max(minor * 0.25, major * 0.5);
	vec3 col = vec3(0.12);
	if (abs(p.z) < w.y * 1.5) { col = vec3(0.9, 0.25, 0.3); alpha = 0.9; }
	if (abs(p.x) < w.x * 1.5) { col = vec3(0.3, 0.5, 0.95); alpha = 0.9; }
	float fade = clamp(1.0 - length(p - F.CamPos.xyz) / 120.0, 0.0, 1.0);
	alpha *= fade;
	if (alpha < 0.01) discard;
	oColor = vec4(col, alpha);
	vec4 clip = F.ViewProj * vec4(p, 1.0);
	// Slightly behind: geometry lying on y = 0 hides the grid instead of z-fighting with it
	gl_FragDepth = min((clip.z / clip.w) * 0.5 + 0.5 + 2e-6, 1.0);
}
)GLSL";

		// Depth-only pass from the sun (also the selection mask with MASK)
		const char* kShadowVS = R"GLSL(
layout(location = 0) in vec3 aPos;
#ifdef SKINNED
layout(location = 6) in ivec4 aBones;
layout(location = 7) in vec4 aWeights;
#endif
void main() {
	mat4 model = P.Model;
#ifdef SKINNED
	mat4 skin = aWeights.x * B[P.BoneOffset + aBones.x] + aWeights.y * B[P.BoneOffset + aBones.y] +
	            aWeights.z * B[P.BoneOffset + aBones.z] + aWeights.w * B[P.BoneOffset + aBones.w];
	if (aWeights.x + aWeights.y + aWeights.z + aWeights.w > 0.0) model = model * skin;
#endif
#ifdef MASK
	gl_Position = F.ViewProj * (model * vec4(aPos, 1.0));
#else
	gl_Position = F.LightViewProj * (model * vec4(aPos, 1.0));
#endif
}
)GLSL";

		const char* kMaskFS = R"GLSL(
layout(location = 0) out vec4 oMask;
void main() { oMask = vec4(1.0); }
)GLSL";

		// Selection outline: 2 px edge around the mask
		const char* kOutlineFS = R"GLSL(
layout(location = 0) in vec2 vNDC;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D tMask;
void main() {
	ivec2 size = textureSize(tMask, 0);
	ivec2 p = ivec2(gl_FragCoord.xy);
	if (texelFetch(tMask, p, 0).r > 0.5) discard;
	float hit = 0.0;
	for (int y = -2; y <= 2; y++)
		for (int x = -2; x <= 2; x++) {
			if (x * x + y * y > 5) continue;
			ivec2 q = clamp(p + ivec2(x, y), ivec2(0), size - 1);
			hit = max(hit, texelFetch(tMask, q, 0).r);
		}
	if (hit < 0.5) discard;
	oColor = F.OutlineColor;
}
)GLSL";

		// ---- Post-processing (own layout: set 0 = up to 2 textures, params in push constants) ----
		const char* kPostCommon = R"GLSL(
#version 450
layout(set = 0, binding = 0) uniform sampler2D tSrc;
layout(set = 0, binding = 1) uniform sampler2D tSrc2;
layout(push_constant) uniform PostParams { vec4 A; vec4 B; vec4 C; ivec4 Flags; } PP;
layout(location = 0) in vec2 vNDC;
layout(location = 0) out vec4 oColor;
)GLSL";

		const char* kPostVS = R"GLSL(
#version 450
layout(location = 0) out vec2 vNDC;
void main() {
	vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vNDC = p * 2.0 - 1.0;
	gl_Position = vec4(vNDC, 0.0, 1.0);
}
)GLSL";

		// 13-tap downsample (CoD: AW), soft threshold on the first level. A.xy = source texel, A.z threshold, Flags.x apply
		const char* kBloomDownFS = R"GLSL(
vec3 S(vec2 uv) { return texture(tSrc, uv).rgb; }
void main() {
	vec2 uv = vNDC * 0.5 + 0.5;
	vec2 t = PP.A.xy;
	vec3 A = S(uv);
	vec3 B = S(uv + t * vec2(-1, -1)), C = S(uv + t * vec2(1, -1)), D = S(uv + t * vec2(-1, 1)), E = S(uv + t * vec2(1, 1));
	vec3 F1 = S(uv + t * vec2(-2, -2)), G = S(uv + t * vec2(0, -2)), H = S(uv + t * vec2(2, -2)), I = S(uv + t * vec2(-2, 0));
	vec3 J = S(uv + t * vec2(2, 0)), K = S(uv + t * vec2(-2, 2)), L2 = S(uv + t * vec2(0, 2)), M2 = S(uv + t * vec2(2, 2));
	vec3 c = A * 0.125 + (B + C + D + E) * 0.09375 + (F1 + H + K + M2) * 0.03125 + (G + I + J + L2) * 0.0625;
	if (PP.Flags.x != 0) {
		float threshold = PP.A.z;
		float brightness = max(c.r, max(c.g, c.b));
		float knee = threshold * 0.5;
		float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
		soft = soft * soft / (4.0 * knee + 1e-5);
		c *= max(max(soft, brightness - threshold) / max(brightness, 1e-5), 0.0);
	}
	oColor = vec4(min(c, vec3(65000.0)), 1.0);
}
)GLSL";

		// 3x3 tent upsample, added onto the next bigger level. A.xy = source texel, A.z radius
		const char* kBloomUpFS = R"GLSL(
void main() {
	vec2 uv = vNDC * 0.5 + 0.5;
	vec2 t = PP.A.xy * PP.A.z;
	vec3 c = texture(tSrc, uv).rgb * 4.0;
	c += (texture(tSrc, uv + vec2(-t.x, 0)).rgb + texture(tSrc, uv + vec2(t.x, 0)).rgb + texture(tSrc, uv + vec2(0, -t.y)).rgb + texture(tSrc, uv + vec2(0, t.y)).rgb) * 2.0;
	c += texture(tSrc, uv - t).rgb + texture(tSrc, uv + t).rgb + texture(tSrc, uv + vec2(-t.x, t.y)).rgb + texture(tSrc, uv + vec2(t.x, -t.y)).rgb;
	oColor = vec4(c / 16.0, 1.0);
}
)GLSL";

		// HDR -> 8 bit. A: bloom intensity, exposure, gamma, CA intensity. B: vignette intensity, roundness, smoothness.
		// Flags: bloom, vignette, chromatic aberration, tone map operator
		const char* kCompositeFS = R"GLSL(
layout(location = 1) out int oEntity;
vec3 ACESFilm(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
vec3 U2(vec3 x) { const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30; return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F; }
vec3 SampleCA(vec2 uv, float intensity) {
	vec2 center = uv - 0.5;
	vec2 offset = center * (dot(center, center) * intensity * 0.04);
	vec2 uvR = uv + offset, uvB = uv - offset;
	vec2 fR = smoothstep(vec2(0.0), vec2(0.02), uvR) * smoothstep(vec2(0.0), vec2(0.02), 1.0 - uvR);
	vec2 fB = smoothstep(vec2(0.0), vec2(0.02), uvB) * smoothstep(vec2(0.0), vec2(0.02), 1.0 - uvB);
	vec3 c = texture(tSrc, uv).rgb;
	return vec3(mix(c.r, texture(tSrc, clamp(uvR, 0.001, 0.999)).r, fR.x * fR.y), c.g, mix(c.b, texture(tSrc, clamp(uvB, 0.001, 0.999)).b, fB.x * fB.y));
}
void main() {
	vec2 uv = vNDC * 0.5 + 0.5;
	vec3 color = PP.Flags.z != 0 ? SampleCA(uv, PP.A.w) : texture(tSrc, uv).rgb;
	if (PP.Flags.x != 0) color += texture(tSrc2, uv).rgb * PP.A.x;
	color *= PP.A.y;
	if (PP.Flags.w == 0) color = ACESFilm(color);
	else if (PP.Flags.w == 1) color = color / (color + 1.0);
	else if (PP.Flags.w == 2) color = U2(color * 2.0) / U2(vec3(11.2));
	color = pow(max(color, vec3(0.0)), vec3(1.0 / max(PP.A.z, 0.01)));
	if (PP.Flags.y != 0) {
		vec2 c = uv - 0.5;
		c.x *= PP.B.y;
		color *= 1.0 - smoothstep(1.0 - PP.B.z, 1.0, length(c) * 2.0 * PP.B.x);
	}
	oColor = vec4(color, 1.0);
	oEntity = 0;
}
)GLSL";

		// ---- Renderer2D batches (own layout: set 0 = 32 textures, view-projection in push constants) ----
		const char* k2DCommon = R"GLSL(
#version 450
layout(push_constant) uniform Push2D { mat4 ViewProj; } P2;
)GLSL";

		const char* kQuadVS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUV;
layout(location = 3) in float aTexIndex;
layout(location = 4) in float aTiling;
layout(location = 5) in int aEntity;
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out float vTexIndex;
layout(location = 3) out float vTiling;
layout(location = 4) out flat int vEntity;
void main() {
	vColor = aColor; vUV = aUV; vTexIndex = aTexIndex; vTiling = aTiling; vEntity = aEntity;
	gl_Position = P2.ViewProj * vec4(aPos, 1.0);
}
)GLSL";

		const char* kQuadFS = R"GLSL(
layout(set = 0, binding = 0) uniform sampler2D uTex[32];
layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in float vTexIndex;
layout(location = 3) in float vTiling;
layout(location = 4) in flat int vEntity;
layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntity;
#define T(i) case i: c *= texture(uTex[i], uv); break;
void main() {
	vec4 c = vColor;
	vec2 uv = vUV * vTiling;
	switch (int(vTexIndex + 0.5)) {
		T(0) T(1) T(2) T(3) T(4) T(5) T(6) T(7) T(8) T(9) T(10) T(11) T(12) T(13) T(14) T(15)
		T(16) T(17) T(18) T(19) T(20) T(21) T(22) T(23) T(24) T(25) T(26) T(27) T(28) T(29) T(30) T(31)
	}
	if (c.a < 0.01) discard;
	oColor = c;
	oEntity = vEntity;
}
)GLSL";

		const char* kCircleVS = R"GLSL(
layout(location = 0) in vec3 aWorld;
layout(location = 1) in vec3 aLocal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aThickness;
layout(location = 4) in float aFade;
layout(location = 5) in int aEntity;
layout(location = 0) out vec3 vLocal;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vThickness;
layout(location = 3) out float vFade;
layout(location = 4) out flat int vEntity;
void main() {
	vLocal = aLocal; vColor = aColor; vThickness = aThickness; vFade = aFade; vEntity = aEntity;
	gl_Position = P2.ViewProj * vec4(aWorld, 1.0);
}
)GLSL";

		const char* kCircleFS = R"GLSL(
layout(location = 0) in vec3 vLocal;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vThickness;
layout(location = 3) in float vFade;
layout(location = 4) in flat int vEntity;
layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntity;
void main() {
	float d = 1.0 - length(vLocal);
	float circle = smoothstep(0.0, vFade, d) * smoothstep(vThickness + vFade, vThickness, d);
	if (circle == 0.0) discard;
	oColor = vec4(vColor.rgb, vColor.a * circle);
	oEntity = vEntity;
}
)GLSL";

		const char* kLineVS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in int aEntity;
layout(location = 0) out vec4 vColor;
layout(location = 1) out flat int vEntity;
void main() {
	vColor = aColor; vEntity = aEntity;
	gl_Position = P2.ViewProj * vec4(aPos, 1.0);
}
)GLSL";

		const char* kLineFS = R"GLSL(
layout(location = 0) in vec4 vColor;
layout(location = 1) in flat int vEntity;
layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntity;
void main() { oColor = vColor; oEntity = vEntity; }
)GLSL";

		std::vector<uint32_t> Compile(const std::string& source, shaderc_shader_kind kind, const char* name,
			const std::vector<const char*>& defines = {}, bool quiet = false) {
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
			for (const char* d : defines) options.AddMacroDefinition(d);
			auto result = compiler.CompileGlslToSpv(source, kind, name, options);
			if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
				if (!quiet) LNX_LOG_ERROR("Vulkan scene shader {0}: {1}", name, result.GetErrorMessage());
				else LNX_LOG_WARN("Vulkan graph shader {0}: {1}", name, result.GetErrorMessage());
				return {};
			}
			return { result.cbegin(), result.cend() };
		}

		// ============================================================================
		// GPU DATA
		// ============================================================================

		struct FrameUBO {
			glm::mat4 View, Proj, ViewProj, InvViewProj;
			glm::vec4 CamPos;
			glm::vec4 EnvParams;
			glm::ivec4 Counts;
			glm::mat4 LightViewProj;
			glm::vec4 ShadowParams;
			glm::vec4 OutlineColor;
			glm::vec4 Viewport;
			glm::vec4 Time;
		};
		constexpr uint32_t kShadowSize = 4096;
		constexpr uint32_t kMaxViews = 2;
		constexpr uint32_t kMaterialTextures = 16;
		constexpr uint32_t kMaxQuads2D = 20000;
		constexpr VkFormat kHDRFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		constexpr VkFormat kLDRFormat = VK_FORMAT_R8G8B8A8_UNORM;
		constexpr VkFormat kEntityFormat = VK_FORMAT_R32_SINT;
		constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

		struct GPULight { glm::vec4 PosRange, DirType, Color, Cone; };
		struct GPUMaterial {
			glm::vec4 Albedo;
			glm::vec4 Emission;    // rgb emission, w roughness multiplier
			glm::vec4 Params;      // metallic, roughness, normal strength, specular
			glm::ivec4 Maps;       // albedo, normal, metallic, roughness
			glm::ivec4 Maps2;      // ao, emission, layered, layered flags | channels
			glm::vec4 UV;          // tiling xy, offset zw
			glm::vec4 Subsurface;  // rgb scatter color, w = metallic multiplier
			glm::vec4 Extra;       // x = subsurface amount
		};
		struct PushData { glm::mat4 Model; int Material; int Entity; int BoneOffset; int Skinned; };
		struct PostPush { glm::vec4 A, B, C; glm::ivec4 Flags; };

		struct Buffer {
			VkBuffer Handle = VK_NULL_HANDLE;
			Allocation Memory;
			VkDeviceSize Size = 0;
		};

		Buffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) {
			auto& ctx = VulkanContext::Get();
			Buffer b;
			VkBufferCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			info.size = size;
			info.usage = usage;
			vkCreateBuffer(ctx.GetDevice(), &info, nullptr, &b.Handle);
			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(ctx.GetDevice(), b.Handle, &req);
			b.Memory = ctx.Allocate(req, props);
			vkBindBufferMemory(ctx.GetDevice(), b.Handle, b.Memory.Memory, b.Memory.Offset);
			b.Size = size;
			return b;
		}

		void DestroyBuffer(Buffer& b) {
			if (!b.Handle) return;
			auto& ctx = VulkanContext::Get();
			const VkBuffer h = b.Handle;
			const Allocation m = b.Memory;
			const VkDevice dev = ctx.GetDevice();
			ctx.DeferDelete([dev, h, m] { vkDestroyBuffer(dev, h, nullptr); VulkanContext::Get().Free(m); });
			b = {};
		}

		Buffer UploadStatic(const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
			auto& ctx = VulkanContext::Get();
			Buffer staging = CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			std::memcpy(staging.Memory.Mapped, data, size);
			Buffer gpu = CreateBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			ctx.ImmediateSubmit([&](VkCommandBuffer cmd) {
				VkBufferCopy copy{ 0, 0, size };
				vkCmdCopyBuffer(cmd, staging.Handle, gpu.Handle, 1, &copy);
			});
			vkDestroyBuffer(ctx.GetDevice(), staging.Handle, nullptr);
			ctx.Free(staging.Memory);
			return gpu;
		}

		struct GPUMesh {
			Buffer Vertices, Indices, Bones;
			uint32_t IndexCount = 0;
			size_t VertexCount = 0;
			uint64_t LastUsed = 0;
		};

		// Host-visible per-frame buffer that grows on demand
		struct DynamicBuffer {
			Buffer Buf;
			void Ensure(VkDeviceSize size, VkBufferUsageFlags usage) {
				if (Buf.Size >= size && Buf.Handle) return;
				DestroyBuffer(Buf);
				Buf = CreateBuffer(std::max<VkDeviceSize>(size, 4096) * 3 / 2, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			}
		};

		// Per view and frame in flight
		struct FrameResources {
			DynamicBuffer Frame, Lights, Bones, Materials, GraphParams, Vertices2D;
			VkDescriptorPool Pool = VK_NULL_HANDLE;
			VkDescriptorSet Set0 = VK_NULL_HANDLE, GraphSet = VK_NULL_HANDLE;
			Buffer Readback;
			bool ReadbackPending = false;
			bool TimestampsPending = false;
		};

		struct ViewTargets {
			Ref<VulkanTexture2D> HDR, Entity, Depth, Output, Mask, SceneColor;
			std::vector<Ref<VulkanTexture2D>> Bloom;
			uint32_t Width = 0, Height = 0;
			FrameResources Frames[kFramesInFlight];
		};

		struct GraphPipeline { VkPipeline Pipeline = VK_NULL_HANDLE; bool Failed = false; };

		struct Renderer {
			bool Initialized = false;
			bool Failed = false;
			VkDescriptorSetLayout Set0Layout = VK_NULL_HANDLE, Set1Layout = VK_NULL_HANDLE, GraphSetLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout PostSetLayout = VK_NULL_HANDLE, Set2DLayout = VK_NULL_HANDLE;
			VkPipelineLayout Layout = VK_NULL_HANDLE, PostLayout = VK_NULL_HANDLE, Layout2D = VK_NULL_HANDLE;
			VkPipeline MeshPipe = VK_NULL_HANDLE, SkinnedPipe = VK_NULL_HANDLE, SkyPipe = VK_NULL_HANDLE, GridPipe = VK_NULL_HANDLE;
			VkPipeline ShadowPipe = VK_NULL_HANDLE, ShadowSkinnedPipe = VK_NULL_HANDLE, OutlinePipe = VK_NULL_HANDLE;
			VkPipeline MaskPipe = VK_NULL_HANDLE, MaskSkinnedPipe = VK_NULL_HANDLE;
			VkPipeline BloomDownPipe = VK_NULL_HANDLE, BloomUpPipe = VK_NULL_HANDLE, CompositePipe = VK_NULL_HANDLE;
			VkPipeline Pipes2D[48] = {};
			VkShaderModule Modules2D[6] = {};   // quad, circle, line: vs, fs
			VkShaderModule MeshVS = VK_NULL_HANDLE, MeshSkinVS = VK_NULL_HANDLE;
			std::unordered_map<uint64_t, GraphPipeline> GraphPipes;
			VkDescriptorPool MaterialPool = VK_NULL_HANDLE;
			VkQueryPool Timestamps = VK_NULL_HANDLE;    // 2 per view and frame in flight
			std::unordered_map<size_t, VkDescriptorSet> MaterialSets;
			ViewTargets Views[kMaxViews];
			Ref<VulkanTexture2D> Shadow;
			Buffer QuadIndices;
			bool WideLines = false;
			VkDeviceSize StorageAlign = 16;

			Ref<VulkanTexture2D> White, FlatNormal, Black;
			Ref<VulkanTexture2D> Env;
			std::string EnvPath;
			float EnvMips = 1.0f;

			std::unordered_map<const Mesh*, GPUMesh> Meshes;
			uint64_t FrameCounter = 0;
		};
		Renderer& R() { static Renderer& r = *new Renderer(); return r; }   // leaked: outlives static destruction

		VkShaderModule Module(const std::vector<uint32_t>& spirv) {
			if (spirv.empty()) return VK_NULL_HANDLE;
			VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			info.codeSize = spirv.size() * 4;
			info.pCode = spirv.data();
			VkShaderModule m = VK_NULL_HANDLE;
			vkCreateShaderModule(VulkanContext::Get().GetDevice(), &info, nullptr, &m);
			return m;
		}

		// ============================================================================
		// PIPELINES
		// ============================================================================

		enum class VertexInput { None, Mesh, MeshSkinned, Position, PositionSkinned, Quad, Circle, Line };
		enum class Blend { None, Alpha, Premultiplied, Additive };

		struct PipelineDesc {
			VkShaderModule VS = VK_NULL_HANDLE, FS = VK_NULL_HANDLE;
			VertexInput Input = VertexInput::None;
			VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			std::vector<VkFormat> Colors;
			bool WriteEntity = false;          // attachment 1 (R32_SINT) written
			VkFormat Depth = VK_FORMAT_UNDEFINED;
			bool DepthTest = false, DepthWrite = false;
			VkCompareOp DepthCompare = VK_COMPARE_OP_LESS;
			Blend BlendMode = Blend::None;
			bool DepthBias = false;
			bool ClipControl = false;          // GL clip space z in [-1, 1]
			bool DynamicLineWidth = false;
			VkPipelineLayout Layout = VK_NULL_HANDLE;
		};

		VkPipeline CreatePipeline(const PipelineDesc& d) {
			auto& ctx = VulkanContext::Get();
			VkPipelineShaderStageCreateInfo stages[2]{};
			stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, d.VS, "main" };
			stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, d.FS, "main" };

			std::vector<VkVertexInputBindingDescription> bindings;
			std::vector<VkVertexInputAttributeDescription> attrs;
			const bool skinned = d.Input == VertexInput::MeshSkinned || d.Input == VertexInput::PositionSkinned;
			switch (d.Input) {
				case VertexInput::Mesh:
				case VertexInput::MeshSkinned:
					bindings.push_back({ 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX });
					attrs.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position) });
					attrs.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal) });
					attrs.push_back({ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, TexCoords) });
					attrs.push_back({ 3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Tangent) });
					attrs.push_back({ 4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Bitangent) });
					break;
				case VertexInput::Position:
				case VertexInput::PositionSkinned:
					bindings.push_back({ 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX });
					attrs.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position) });
					break;
				case VertexInput::Quad:
					bindings.push_back({ 0, sizeof(QuadVertex), VK_VERTEX_INPUT_RATE_VERTEX });
					attrs.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(QuadVertex, Position) });
					attrs.push_back({ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(QuadVertex, Color) });
					attrs.push_back({ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(QuadVertex, TexCoord) });
					attrs.push_back({ 3, 0, VK_FORMAT_R32_SFLOAT, offsetof(QuadVertex, TexIndex) });
					attrs.push_back({ 4, 0, VK_FORMAT_R32_SFLOAT, offsetof(QuadVertex, TilingFactor) });
					attrs.push_back({ 5, 0, VK_FORMAT_R32_SINT, offsetof(QuadVertex, EntityID) });
					break;
				case VertexInput::Circle:
					bindings.push_back({ 0, sizeof(CircleVertex), VK_VERTEX_INPUT_RATE_VERTEX });
					attrs.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CircleVertex, WorldPosition) });
					attrs.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CircleVertex, LocalPosition) });
					attrs.push_back({ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(CircleVertex, Color) });
					attrs.push_back({ 3, 0, VK_FORMAT_R32_SFLOAT, offsetof(CircleVertex, Thickness) });
					attrs.push_back({ 4, 0, VK_FORMAT_R32_SFLOAT, offsetof(CircleVertex, Fade) });
					attrs.push_back({ 5, 0, VK_FORMAT_R32_SINT, offsetof(CircleVertex, EntityID) });
					break;
				case VertexInput::Line:
					bindings.push_back({ 0, sizeof(LineVertex), VK_VERTEX_INPUT_RATE_VERTEX });
					attrs.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LineVertex, Position) });
					attrs.push_back({ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LineVertex, Color) });
					attrs.push_back({ 2, 0, VK_FORMAT_R32_SINT, offsetof(LineVertex, EntityID) });
					break;
				case VertexInput::None: break;
			}
			if (skinned) {
				bindings.push_back({ 1, sizeof(glm::ivec4) + sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX });
				attrs.push_back({ 6, 1, VK_FORMAT_R32G32B32A32_SINT, 0 });
				attrs.push_back({ 7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::ivec4) });
			}
			VkPipelineVertexInputStateCreateInfo vertex{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
			vertex.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
			vertex.pVertexBindingDescriptions = bindings.data();
			vertex.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
			vertex.pVertexAttributeDescriptions = attrs.data();

			VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
			ia.topology = d.Topology;

			// GL clip space: z in [-1, 1] (glm projections) without touching the matrices
			VkPipelineViewportDepthClipControlCreateInfoEXT clip{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
			clip.negativeOneToOne = VK_TRUE;
			VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
			vp.viewportCount = 1;
			vp.scissorCount = 1;
			if (ctx.HasDepthClipControl() && d.ClipControl) vp.pNext = &clip;

			VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
			rs.polygonMode = VK_POLYGON_MODE_FILL;
			rs.cullMode = VK_CULL_MODE_NONE;     // double sided like the deferred path
			// Same rows as GL (positive viewport) but Vulkan's area formula has the opposite sign:
			// GL's counter-clockwise front faces are clockwise here
			rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rs.lineWidth = 1.0f;
			if (d.DepthBias) {
				rs.depthBiasEnable = VK_TRUE;
				rs.depthBiasConstantFactor = 1.5f;
				rs.depthBiasSlopeFactor = 2.0f;
			}
			VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
			ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
			ds.depthTestEnable = d.DepthTest ? VK_TRUE : VK_FALSE;
			ds.depthWriteEnable = d.DepthWrite ? VK_TRUE : VK_FALSE;
			ds.depthCompareOp = d.DepthCompare;

			std::vector<VkPipelineColorBlendAttachmentState> blend(d.Colors.size());
			for (size_t i = 0; i < blend.size(); i++) {
				blend[i] = {};
				const bool isEntity = d.Colors[i] == kEntityFormat;
				blend[i].colorWriteMask = isEntity ? (d.WriteEntity ? VK_COLOR_COMPONENT_R_BIT : 0) : 0xF;
				if (isEntity || d.BlendMode == Blend::None) continue;
				blend[i].blendEnable = VK_TRUE;
				blend[i].colorBlendOp = VK_BLEND_OP_ADD;
				blend[i].alphaBlendOp = VK_BLEND_OP_ADD;
				switch (d.BlendMode) {
					case Blend::Alpha:
						blend[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
						blend[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
						blend[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
						blend[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
						break;
					case Blend::Premultiplied:
						blend[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
						blend[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
						blend[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
						blend[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
						break;
					case Blend::Additive:
						blend[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
						blend[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
						blend[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
						blend[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
						break;
					case Blend::None: break;
				}
			}
			// Bloom upsample adds onto the destination as is
			VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
			cb.attachmentCount = static_cast<uint32_t>(blend.size());
			cb.pAttachments = blend.data();
			std::vector<VkDynamicState> dyn = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			if (d.DynamicLineWidth) dyn.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
			VkPipelineDynamicStateCreateInfo dynamic{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
			dynamic.dynamicStateCount = static_cast<uint32_t>(dyn.size());
			dynamic.pDynamicStates = dyn.data();

			VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			rendering.colorAttachmentCount = static_cast<uint32_t>(d.Colors.size());
			rendering.pColorAttachmentFormats = d.Colors.data();
			rendering.depthAttachmentFormat = d.Depth;

			VkGraphicsPipelineCreateInfo info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
			info.pNext = &rendering;
			info.stageCount = d.FS ? 2 : 1;
			info.pStages = stages;
			info.pVertexInputState = &vertex;
			info.pInputAssemblyState = &ia;
			info.pViewportState = &vp;
			info.pRasterizationState = &rs;
			info.pMultisampleState = &ms;
			info.pDepthStencilState = &ds;
			info.pColorBlendState = &cb;
			info.pDynamicState = &dynamic;
			info.layout = d.Layout;
			VkPipeline pipeline = VK_NULL_HANDLE;
			if (vkCreateGraphicsPipelines(ctx.GetDevice(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
				LNX_LOG_ERROR("Vulkan scene renderer: pipeline creation failed");
			return pipeline;
		}

		// Opaque HDR pass / transparent pass attachments
		PipelineDesc HDRPass(VkShaderModule vs, VkShaderModule fs, VertexInput input) {
			PipelineDesc d;
			d.VS = vs; d.FS = fs; d.Input = input;
			d.Colors = { kHDRFormat, kEntityFormat };
			d.WriteEntity = true;
			d.Depth = kDepthFormat;
			d.DepthTest = true; d.DepthWrite = true;
			d.ClipControl = true;
			d.Layout = R().Layout;
			return d;
		}
		// Composite / overlay pass attachments (8-bit output + entity IDs + depth)
		PipelineDesc LDRPass(VkShaderModule vs, VkShaderModule fs) {
			PipelineDesc d;
			d.VS = vs; d.FS = fs;
			d.Colors = { kLDRFormat, kEntityFormat };
			d.Depth = kDepthFormat;
			d.Layout = R().Layout;
			return d;
		}

		Ref<VulkanTexture2D> SolidTexture(uint32_t rgba) {
			RHI::TextureDesc desc(1, 1, RHI::TextureFormat::RGBA8);
			return CreateRef<VulkanTexture2D>(desc, &rgba, sizeof(rgba));
		}

		VkDescriptorSetLayout SamplerLayout(uint32_t count, bool arrayBinding) {
			std::vector<VkDescriptorSetLayoutBinding> b;
			if (arrayBinding) b.push_back({ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
			else for (uint32_t i = 0; i < count; i++) b.push_back({ i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
			VkDescriptorSetLayoutCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			info.bindingCount = static_cast<uint32_t>(b.size());
			info.pBindings = b.data();
			VkDescriptorSetLayout layout = VK_NULL_HANDLE;
			vkCreateDescriptorSetLayout(VulkanContext::Get().GetDevice(), &info, nullptr, &layout);
			return layout;
		}

		VkPipelineLayout PipelineLayout(const std::vector<VkDescriptorSetLayout>& sets, uint32_t pushSize, VkShaderStageFlags pushStages) {
			VkPushConstantRange push{ pushStages, 0, pushSize };
			VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
			info.setLayoutCount = static_cast<uint32_t>(sets.size());
			info.pSetLayouts = sets.data();
			info.pushConstantRangeCount = 1;
			info.pPushConstantRanges = &push;
			VkPipelineLayout layout = VK_NULL_HANDLE;
			vkCreatePipelineLayout(VulkanContext::Get().GetDevice(), &info, nullptr, &layout);
			return layout;
		}

		bool Init() {
			auto& r = R();
			auto& ctx = VulkanContext::Get();
			const VkDevice dev = ctx.GetDevice();

			// Set 0: frame UBO, lights, bones, materials, environment, sun shadow, scene color
			const VkShaderStageFlags vf = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			VkDescriptorSetLayoutBinding b0[7] = {
				{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, vf, nullptr },
				{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },   // bones
				{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			};
			VkDescriptorSetLayoutCreateInfo l0{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			l0.bindingCount = 7;
			l0.pBindings = b0;
			vkCreateDescriptorSetLayout(dev, &l0, nullptr, &r.Set0Layout);
			// Set 1: material textures (classic maps 0-6, graph images 0-11)
			r.Set1Layout = SamplerLayout(kMaterialTextures, false);
			// Set 2: graph parameters (dynamic offset per draw)
			VkDescriptorSetLayoutBinding g{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
			VkDescriptorSetLayoutCreateInfo lg{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			lg.bindingCount = 1;
			lg.pBindings = &g;
			vkCreateDescriptorSetLayout(dev, &lg, nullptr, &r.GraphSetLayout);
			r.Layout = PipelineLayout({ r.Set0Layout, r.Set1Layout, r.GraphSetLayout }, sizeof(PushData), vf);
			r.PostSetLayout = SamplerLayout(2, false);
			r.PostLayout = PipelineLayout({ r.PostSetLayout }, sizeof(PostPush), VK_SHADER_STAGE_FRAGMENT_BIT);
			r.Set2DLayout = SamplerLayout(32, true);
			r.Layout2D = PipelineLayout({ r.Set2DLayout }, sizeof(glm::mat4), VK_SHADER_STAGE_VERTEX_BIT);

			VkPhysicalDeviceFeatures features{};
			vkGetPhysicalDeviceFeatures(ctx.GetPhysicalDevice(), &features);
			r.WideLines = features.wideLines == VK_TRUE;
			r.StorageAlign = std::max<VkDeviceSize>(ctx.GetProperties().limits.minStorageBufferOffsetAlignment, 16);

			const std::string common = kCommon;
			const std::string shading = common + kShading;
			r.MeshVS = Module(Compile(common + kMeshVS, shaderc_vertex_shader, "mesh.vert"));
			r.MeshSkinVS = Module(Compile(common + kMeshVS, shaderc_vertex_shader, "mesh_skinned.vert", { "SKINNED" }));
			VkShaderModule meshFS = Module(Compile(shading + kMeshFS, shaderc_fragment_shader, "mesh.frag"));
			VkShaderModule fullVS = Module(Compile(common + kFullscreenVS, shaderc_vertex_shader, "fullscreen.vert"));
			VkShaderModule skyFS = Module(Compile(common + kSkyFS, shaderc_fragment_shader, "sky.frag"));
			VkShaderModule gridFS = Module(Compile(common + kGridFS, shaderc_fragment_shader, "grid.frag"));
			VkShaderModule shadowVS = Module(Compile(common + kShadowVS, shaderc_vertex_shader, "shadow.vert"));
			VkShaderModule shadowSkinVS = Module(Compile(common + kShadowVS, shaderc_vertex_shader, "shadow_skinned.vert", { "SKINNED" }));
			VkShaderModule outlineFS = Module(Compile(common + kOutlineFS, shaderc_fragment_shader, "outline.frag"));
			VkShaderModule maskVS = Module(Compile(common + kShadowVS, shaderc_vertex_shader, "mask.vert", { "MASK" }));
			VkShaderModule maskSkinVS = Module(Compile(common + kShadowVS, shaderc_vertex_shader, "mask_skinned.vert", { "SKINNED", "MASK" }));
			VkShaderModule maskFS = Module(Compile(common + kMaskFS, shaderc_fragment_shader, "mask.frag"));
			const std::string post = kPostCommon;
			VkShaderModule postVS = Module(Compile(kPostVS, shaderc_vertex_shader, "post.vert"));
			VkShaderModule bloomDownFS = Module(Compile(post + kBloomDownFS, shaderc_fragment_shader, "bloom_down.frag"));
			VkShaderModule bloomUpFS = Module(Compile(post + kBloomUpFS, shaderc_fragment_shader, "bloom_up.frag"));
			VkShaderModule compositeFS = Module(Compile(post + kCompositeFS, shaderc_fragment_shader, "composite.frag"));
			const std::string c2d = k2DCommon;
			r.Modules2D[0] = Module(Compile(c2d + kQuadVS, shaderc_vertex_shader, "quad2d.vert"));
			r.Modules2D[1] = Module(Compile(c2d + kQuadFS, shaderc_fragment_shader, "quad2d.frag"));
			r.Modules2D[2] = Module(Compile(c2d + kCircleVS, shaderc_vertex_shader, "circle2d.vert"));
			r.Modules2D[3] = Module(Compile(c2d + kCircleFS, shaderc_fragment_shader, "circle2d.frag"));
			r.Modules2D[4] = Module(Compile(c2d + kLineVS, shaderc_vertex_shader, "line2d.vert"));
			r.Modules2D[5] = Module(Compile(c2d + kLineFS, shaderc_fragment_shader, "line2d.frag"));
			const VkShaderModule all[] = { r.MeshVS, r.MeshSkinVS, meshFS, fullVS, skyFS, gridFS, shadowVS, shadowSkinVS, outlineFS, maskVS, maskSkinVS, maskFS,
				postVS, bloomDownFS, bloomUpFS, compositeFS, r.Modules2D[0], r.Modules2D[1], r.Modules2D[2], r.Modules2D[3], r.Modules2D[4], r.Modules2D[5] };
			for (VkShaderModule m : all) if (!m) return false;

			r.MeshPipe = CreatePipeline(HDRPass(r.MeshVS, meshFS, VertexInput::Mesh));
			r.SkinnedPipe = CreatePipeline(HDRPass(r.MeshSkinVS, meshFS, VertexInput::MeshSkinned));
			{
				PipelineDesc d = HDRPass(fullVS, skyFS, VertexInput::None);
				d.DepthWrite = false;
				d.DepthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
				d.ClipControl = false;
				r.SkyPipe = CreatePipeline(d);
			}
			{
				PipelineDesc d = LDRPass(fullVS, gridFS);
				d.DepthTest = true;
				d.BlendMode = Blend::Alpha;
				r.GridPipe = CreatePipeline(d);
			}
			{
				PipelineDesc d = LDRPass(fullVS, outlineFS);
				d.BlendMode = Blend::Alpha;
				r.OutlinePipe = CreatePipeline(d);
			}
			for (int s = 0; s < 2; s++) {
				PipelineDesc d;
				d.VS = s ? shadowSkinVS : shadowVS;
				d.Input = s ? VertexInput::PositionSkinned : VertexInput::Position;
				d.Depth = kDepthFormat;
				d.DepthTest = true; d.DepthWrite = true;
				d.DepthBias = true;
				d.ClipControl = true;
				d.Layout = r.Layout;
				(s ? r.ShadowSkinnedPipe : r.ShadowPipe) = CreatePipeline(d);

				PipelineDesc m;
				m.VS = s ? maskSkinVS : maskVS;
				m.FS = maskFS;
				m.Input = d.Input;
				m.Colors = { kLDRFormat };
				m.ClipControl = true;
				m.Layout = r.Layout;
				(s ? r.MaskSkinnedPipe : r.MaskPipe) = CreatePipeline(m);
			}
			{
				PipelineDesc d;
				d.VS = postVS; d.FS = bloomDownFS;
				d.Colors = { kHDRFormat };
				d.Layout = r.PostLayout;
				r.BloomDownPipe = CreatePipeline(d);
				d.FS = bloomUpFS;
				d.BlendMode = Blend::Additive;
				r.BloomUpPipe = CreatePipeline(d);
				PipelineDesc c;
				c.VS = postVS; c.FS = compositeFS;
				c.Colors = { kLDRFormat, kEntityFormat };
				c.Depth = kDepthFormat;
				c.Layout = r.PostLayout;
				r.CompositePipe = CreatePipeline(c);
			}
			for (VkShaderModule m : { meshFS, fullVS, skyFS, gridFS, shadowVS, shadowSkinVS, outlineFS, maskVS, maskSkinVS, maskFS, postVS, bloomDownFS, bloomUpFS, compositeFS })
				vkDestroyShaderModule(dev, m, nullptr);
			for (VkPipeline p : { r.MeshPipe, r.SkinnedPipe, r.SkyPipe, r.GridPipe, r.OutlinePipe, r.ShadowPipe, r.ShadowSkinnedPipe, r.MaskPipe, r.MaskSkinnedPipe,
				r.BloomDownPipe, r.BloomUpPipe, r.CompositePipe })
				if (!p) return false;

			r.Shadow = CreateRef<VulkanTexture2D>(RHI::TextureDesc::RenderTarget(kShadowSize, kShadowSize, RHI::TextureFormat::Depth32F));

			const VkDescriptorPoolSize sizes[] = {
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 4 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 * 32 } };
			for (auto& view : r.Views)
				for (auto& f : view.Frames) {
					VkDescriptorPoolCreateInfo pool{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
					pool.maxSets = 600;
					pool.poolSizeCount = 4;
					pool.pPoolSizes = sizes;
					vkCreateDescriptorPool(dev, &pool, nullptr, &f.Pool);
					f.Readback = CreateBuffer(16, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				}
			const VkDescriptorPoolSize matSizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 * kMaterialTextures } };
			VkDescriptorPoolCreateInfo matPool{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			matPool.maxSets = 4096;
			matPool.poolSizeCount = 1;
			matPool.pPoolSizes = matSizes;
			vkCreateDescriptorPool(dev, &matPool, nullptr, &r.MaterialPool);

			if (ctx.GetProperties().limits.timestampComputeAndGraphics) {
				VkQueryPoolCreateInfo query{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
				query.queryType = VK_QUERY_TYPE_TIMESTAMP;
				query.queryCount = 2 * kFramesInFlight * kMaxViews;
				vkCreateQueryPool(dev, &query, nullptr, &r.Timestamps);
			}

			// Quad index buffer shared by every 2D batch (quads and circles)
			std::vector<uint32_t> indices(kMaxQuads2D * 6);
			for (uint32_t q = 0; q < kMaxQuads2D; q++) {
				const uint32_t v = q * 4;
				const uint32_t idx[6] = { v, v + 1, v + 2, v + 2, v + 3, v };
				std::memcpy(&indices[q * 6], idx, sizeof(idx));
			}
			r.QuadIndices = UploadStatic(indices.data(), indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

			r.White = SolidTexture(0xFFFFFFFF);
			r.FlatNormal = SolidTexture(0xFFFF8080);
			r.Black = SolidTexture(0xFF000000);
			LNX_LOG_INFO("Vulkan scene renderer ready");
			return true;
		}

		// 2D pipelines are created on first use: kind (quad, circle, line) x layer x blend x depth test x depth write
		VkPipeline Get2DPipeline(int kind, bool overlay, bool additive, bool depthTest, bool depthWrite) {
			auto& r = R();
			const int index = (((kind * 2 + (overlay ? 1 : 0)) * 2 + (additive ? 1 : 0)) * 2 + (depthTest ? 1 : 0)) * 2 + (depthWrite ? 1 : 0);
			if (r.Pipes2D[index]) return r.Pipes2D[index];
			PipelineDesc d;
			d.VS = r.Modules2D[kind * 2];
			d.FS = r.Modules2D[kind * 2 + 1];
			d.Input = kind == 0 ? VertexInput::Quad : kind == 1 ? VertexInput::Circle : VertexInput::Line;
			d.Topology = kind == 2 ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			d.Colors = { overlay ? kLDRFormat : kHDRFormat, kEntityFormat };
			d.WriteEntity = true;
			d.Depth = kDepthFormat;
			d.DepthTest = depthTest;
			d.DepthWrite = depthWrite;
			d.DepthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
			d.BlendMode = additive ? Blend::Additive : Blend::Alpha;
			d.ClipControl = true;
			d.DynamicLineWidth = kind == 2 && r.WideLines;
			d.Layout = r.Layout2D;
			r.Pipes2D[index] = CreatePipeline(d);
			return r.Pipes2D[index];
		}

		// Node graph material pipeline: compiled on first use for (code, forward, skinned)
		VkPipeline GetGraphPipeline(const MaterialGraph::CompiledGraph& compiled, bool forward, bool skinned) {
			auto& r = R();
			const uint64_t key = compiled.CodeHash * 4 + (forward ? 2 : 0) + (skinned ? 1 : 0);
			GraphPipeline& gp = r.GraphPipes[key];
			if (gp.Pipeline || gp.Failed) return gp.Pipeline;
			const std::string source = std::string(kCommon) + kShading + kGraphHeader + compiled.Code + kGraphMain;
			char name[64];
			snprintf(name, sizeof(name), "graph_%016llx_%s.frag", static_cast<unsigned long long>(compiled.CodeHash), forward ? "fwd" : "opq");
			std::vector<const char*> defines;
			if (forward) defines.push_back("FORWARD");
			VkShaderModule fs = Module(Compile(source, shaderc_fragment_shader, name, defines, true));
			if (!fs) { gp.Failed = true; return VK_NULL_HANDLE; }
			PipelineDesc d = HDRPass(skinned ? r.MeshSkinVS : r.MeshVS, fs, skinned ? VertexInput::MeshSkinned : VertexInput::Mesh);
			if (forward) {
				d.DepthWrite = false;
				d.BlendMode = Blend::Premultiplied;
			}
			gp.Pipeline = CreatePipeline(d);
			gp.Failed = gp.Pipeline == VK_NULL_HANDLE;
			vkDestroyShaderModule(VulkanContext::Get().GetDevice(), fs, nullptr);
			return gp.Pipeline;
		}

		// Loads the scene HDRI as a mipmapped RGBA16F equirect (max 2048 wide: sky + IBL)
		void UpdateEnvironment() {
			auto& r = R();
			Ref<EnvironmentMap> env = SkyboxRenderer::GetGlobalEnvironment();
			const std::string path = env ? env->GetPath() : std::string();
			if (path == r.EnvPath) return;
			r.EnvPath = path;
			r.Env.reset();
			if (path.empty()) return;
			int w = 0, h = 0, c = 0;
			stbi_set_flip_vertically_on_load_thread(0);
			float* data = stbi_loadf(path.c_str(), &w, &h, &c, 4);
			stbi_set_flip_vertically_on_load_thread(1);
			if (!data) { LNX_LOG_WARN("Vulkan: could not load HDRI {0}", path); return; }
			int step = 1;
			while (w / step > 2048) step *= 2;
			const int dw = w / step, dh = h / step;
			std::vector<uint16_t> half(static_cast<size_t>(dw) * dh * 4);
			for (int y = 0; y < dh; y++)
				for (int x = 0; x < dw; x++) {
					glm::vec4 sum(0.0f);
					for (int sy = 0; sy < step; sy++)
						for (int sx = 0; sx < step; sx++) {
							const float* p = data + ((static_cast<size_t>(y) * step + sy) * w + (static_cast<size_t>(x) * step + sx)) * 4;
							sum += glm::vec4(p[0], p[1], p[2], 1.0f);
						}
					sum /= static_cast<float>(step * step);
					for (int k = 0; k < 4; k++) half[(static_cast<size_t>(y) * dw + x) * 4 + k] = glm::packHalf1x16(std::min(sum[k], 60000.0f));
				}
			stbi_image_free(data);
			RHI::TextureDesc desc(dw, dh, RHI::TextureFormat::RGBA16F);
			desc.GenerateMipmaps = true;
			r.Env = CreateRef<VulkanTexture2D>(desc, half.data(), half.size() * sizeof(uint16_t));
			r.EnvMips = static_cast<float>(r.Env->GetMipLevels());
		}

		uint32_t MipCount(uint32_t w, uint32_t h) {
			uint32_t m = 1;
			while ((std::max(w, h) >> m) > 0) m++;
			return m;
		}

		void EnsureTargets(ViewTargets& t, uint32_t w, uint32_t h) {
			if (t.HDR && t.Width == w && t.Height == h) return;
			t.Width = w;
			t.Height = h;
			t.SceneColor.reset();
			t.Bloom.clear();
			if (t.HDR) {
				for (auto* tex : { &t.HDR, &t.Entity, &t.Depth, &t.Output, &t.Mask }) (*tex)->Resize(w, h);
				return;
			}
			t.HDR = CreateRef<VulkanTexture2D>(RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::RGBA16F));
			t.Entity = CreateRef<VulkanTexture2D>(RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::R32I));
			t.Depth = CreateRef<VulkanTexture2D>(RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::Depth32F));
			t.Output = CreateRef<VulkanTexture2D>(RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::RGBA8));
			t.Mask = CreateRef<VulkanTexture2D>(RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::RGBA8));
		}

		Ref<VulkanTexture2D> GetSceneColor(ViewTargets& t) {
			if (!t.SceneColor) {
				RHI::TextureDesc desc = RHI::TextureDesc::RenderTarget(t.Width, t.Height, RHI::TextureFormat::RGBA16F);
				desc.MipLevels = MipCount(t.Width, t.Height);
				t.SceneColor = CreateRef<VulkanTexture2D>(desc);
			}
			return t.SceneColor;
		}

		void EnsureBloom(ViewTargets& t, int levels) {
			if (static_cast<int>(t.Bloom.size()) == levels) return;
			t.Bloom.clear();
			for (int i = 0; i < levels; i++) {
				const uint32_t w = std::max(1u, t.Width >> (i + 1)), h = std::max(1u, t.Height >> (i + 1));
				t.Bloom.push_back(CreateRef<VulkanTexture2D>(RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::RGBA16F)));
			}
		}

		GPUMesh* GetMesh(const Mesh& mesh) {
			auto& r = R();
			auto it = r.Meshes.find(&mesh);
			// Same address reused by another mesh: rebuild
			if (it != r.Meshes.end() && (it->second.VertexCount != mesh.GetVertices().size() || it->second.IndexCount != mesh.GetIndices().size())) {
				DestroyBuffer(it->second.Vertices); DestroyBuffer(it->second.Indices); DestroyBuffer(it->second.Bones);
				r.Meshes.erase(it);
				it = r.Meshes.end();
			}
			if (it != r.Meshes.end()) { it->second.LastUsed = r.FrameCounter; return &it->second; }
			if (mesh.GetVertices().empty() || mesh.GetIndices().empty()) return nullptr;
			GPUMesh g;
			g.Vertices = UploadStatic(mesh.GetVertices().data(), mesh.GetVertices().size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			g.Indices = UploadStatic(mesh.GetIndices().data(), mesh.GetIndices().size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
			if (mesh.IsSkinned() && mesh.GetBoneIDs().size() == mesh.GetVertices().size()) {
				struct BoneVertex { glm::ivec4 IDs; glm::vec4 Weights; };
				std::vector<BoneVertex> bones(mesh.GetVertices().size());
				for (size_t i = 0; i < bones.size(); i++) bones[i] = { mesh.GetBoneIDs()[i], mesh.GetBoneWeights()[i] };
				g.Bones = UploadStatic(bones.data(), bones.size() * sizeof(BoneVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			}
			g.IndexCount = static_cast<uint32_t>(mesh.GetIndices().size());
			g.VertexCount = mesh.GetVertices().size();
			g.LastUsed = r.FrameCounter;
			return &(r.Meshes[&mesh] = g);
		}

		VulkanTexture2D* VkTex(const Ref<Texture2D>& t) {
			if (!t || !t->IsLoaded()) return nullptr;
			return dynamic_cast<VulkanTexture2D*>(t->GetRHITexture());
		}

		// Descriptor set for a texture combination (cached; missing slots: white, flat normal at 1)
		VkDescriptorSet MaterialSet(VulkanTexture2D* const* tex, uint32_t count, bool flatNormalAt1) {
			auto& r = R();
			VulkanTexture2D* slots[kMaterialTextures] = {};
			for (uint32_t i = 0; i < count && i < kMaterialTextures; i++) slots[i] = tex[i];
			size_t key = flatNormalAt1 ? 1469598103934665603ull : 7809847782465536322ull;
			for (uint32_t i = 0; i < kMaterialTextures; i++) key = (key ^ reinterpret_cast<size_t>(slots[i] ? slots[i]->GetView() : VK_NULL_HANDLE)) * 1099511628211ull;
			auto it = r.MaterialSets.find(key);
			if (it != r.MaterialSets.end()) return it->second;
			auto& ctx = VulkanContext::Get();
			VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			alloc.descriptorPool = r.MaterialPool;
			alloc.descriptorSetCount = 1;
			alloc.pSetLayouts = &r.Set1Layout;
			VkDescriptorSet set = VK_NULL_HANDLE;
			if (vkAllocateDescriptorSets(ctx.GetDevice(), &alloc, &set) != VK_SUCCESS) {
				// Pool full (textures reloaded many times): start over. Sets still referenced by
				// in-flight frames stay valid until the reset, which waits for the device first.
				vkDeviceWaitIdle(ctx.GetDevice());
				vkResetDescriptorPool(ctx.GetDevice(), r.MaterialPool, 0);
				r.MaterialSets.clear();
				vkAllocateDescriptorSets(ctx.GetDevice(), &alloc, &set);
			}
			VkDescriptorImageInfo images[kMaterialTextures];
			VkWriteDescriptorSet writes[kMaterialTextures];
			for (uint32_t i = 0; i < kMaterialTextures; i++) {
				VulkanTexture2D* t = slots[i] ? slots[i] : ((i == 1 && flatNormalAt1) ? r.FlatNormal.get() : r.White.get());
				images[i] = { t->GetDefaultSampler(), t->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
				writes[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, i, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &images[i], nullptr, nullptr };
			}
			vkUpdateDescriptorSets(ctx.GetDevice(), kMaterialTextures, writes, 0, nullptr);
			r.MaterialSets[key] = set;
			return set;
		}

		struct DrawItem {
			const GPUMesh* Mesh = nullptr;
			glm::mat4 Model;
			int Material = 0;
			int Entity = -1;
			int BoneOffset = 0;
			bool Skinned = false;
			VkDescriptorSet Textures = VK_NULL_HANDLE;
			VkPipeline Pipeline = VK_NULL_HANDLE;   // graph material pipeline (null = classic)
			uint32_t ParamsOffset = 0;              // graph params (dynamic offset)
			float Distance = 0.0f;                  // transparent sorting
		};

		// Fills a GPU material from the engine's material data; returns the texture set
		template<typename UniformData>
		VkDescriptorSet FillMaterial(GPUMaterial& out, const UniformData& u, const Ref<MaterialAsset>& asset) {
			out.Albedo = u.Albedo;
			out.Emission = glm::vec4(u.EmissionColor * u.EmissionIntensity, u.RoughnessMultiplier);
			out.Params = glm::vec4(u.Metallic, u.Roughness, u.NormalIntensity, u.Specular);
			out.UV = (u.UVTilingOffset.x == 0.0f && u.UVTilingOffset.y == 0.0f) ? glm::vec4(1, 1, 0, 0) : u.UVTilingOffset;
			out.Subsurface = glm::vec4(glm::vec3(u.SubsurfaceColorAmount), u.MetallicMultiplier);
			out.Extra = glm::vec4(u.SubsurfaceColorAmount.a, 0.0f, 0.0f, 0.0f);
			VulkanTexture2D* tex[7] = {};
			if (asset) {
				tex[0] = u.UseAlbedoMap ? VkTex(asset->GetAlbedoMap()) : nullptr;
				tex[1] = u.UseNormalMap ? VkTex(asset->GetNormalMap()) : nullptr;
				tex[2] = u.UseMetallicMap ? VkTex(asset->GetMetallicMap()) : nullptr;
				tex[3] = u.UseRoughnessMap ? VkTex(asset->GetRoughnessMap()) : nullptr;
				tex[4] = u.UseAOMap ? VkTex(asset->GetAOMap()) : nullptr;
				tex[5] = u.UseEmissionMap ? VkTex(asset->GetEmissionMap()) : nullptr;
				tex[6] = u.UseLayeredTexture ? VkTex(asset->GetLayeredTextureConfig().Texture) : nullptr;
			}
			out.Maps = glm::ivec4(tex[0] != nullptr, tex[1] != nullptr, tex[2] != nullptr, tex[3] != nullptr);
			int layeredFlags = (u.LayeredUseMetallic ? 1 : 0) | (u.LayeredUseRoughness ? 2 : 0) | (u.LayeredUseAO ? 4 : 0) |
				((u.LayeredMetallicChannel & 3) << 4) | ((u.LayeredRoughnessChannel & 3) << 6) | ((u.LayeredAOChannel & 3) << 8);
			out.Maps2 = glm::ivec4(tex[4] != nullptr, tex[5] != nullptr, tex[6] != nullptr, layeredFlags);
			return MaterialSet(tex, 7, true);
		}

		// Graph material of an asset for this draw (null pipeline = use the classic path)
		struct GraphBinding { VkPipeline Pipeline = VK_NULL_HANDLE; VkDescriptorSet Textures = VK_NULL_HANDLE; uint32_t ParamsOffset = 0; bool Forward = false; };

		void BindViewport(VkCommandBuffer cmd, uint32_t w, uint32_t h) {
			// Positive height: NDC -1 -> row 0, so image rows match the GL framebuffer
			VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
			VkRect2D scissor{ { 0, 0 }, { w, h } };
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);
		}

		VkRenderingAttachmentInfo Attachment(VulkanTexture2D* tex, VkImageLayout layout, bool clear) {
			VkRenderingAttachmentInfo a{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
			a.imageView = tex->GetView();
			a.imageLayout = layout;
			a.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
			a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			return a;
		}

		void BeginPass(VkCommandBuffer cmd, uint32_t w, uint32_t h, const VkRenderingAttachmentInfo* colors, uint32_t colorCount, const VkRenderingAttachmentInfo* depth) {
			VkRenderingInfo info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
			info.renderArea = { { 0, 0 }, { w, h } };
			info.layerCount = 1;
			info.colorAttachmentCount = colorCount;
			info.pColorAttachments = colors;
			info.pDepthAttachment = depth;
			vkCmdBeginRendering(cmd, &info);
			BindViewport(cmd, w, h);
		}

		// Post-processing descriptor set (per frame, from the view-frame pool)
		VkDescriptorSet PostSet(FrameResources& fr, VulkanTexture2D* a, VulkanTexture2D* b) {
			auto& r = R();
			VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			alloc.descriptorPool = fr.Pool;
			alloc.descriptorSetCount = 1;
			alloc.pSetLayouts = &r.PostSetLayout;
			VkDescriptorSet set = VK_NULL_HANDLE;
			if (vkAllocateDescriptorSets(VulkanContext::Get().GetDevice(), &alloc, &set) != VK_SUCCESS) return VK_NULL_HANDLE;
			const VkSampler linear = VulkanContext::Get().GetSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
			VkDescriptorImageInfo images[2] = {
				{ linear, a->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
				{ linear, (b ? b : a)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			};
			VkWriteDescriptorSet writes[2] = {
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &images[0], nullptr, nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &images[1], nullptr, nullptr },
			};
			vkUpdateDescriptorSets(VulkanContext::Get().GetDevice(), 2, writes, 0, nullptr);
			return set;
		}

		// Copies the opaque HDR image into the mipmapped scene color (refraction source)
		void CopySceneColor(VkCommandBuffer cmd, ViewTargets& t) {
			Ref<VulkanTexture2D> dst = GetSceneColor(t);
			const uint32_t mips = dst->GetMipLevels();
			t.HDR->Transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			dst->Transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkImageBlit blit{};
			blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			blit.srcOffsets[1] = { static_cast<int32_t>(t.Width), static_cast<int32_t>(t.Height), 1 };
			blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			blit.dstOffsets[1] = blit.srcOffsets[1];
			vkCmdBlitImage(cmd, t.HDR->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
			auto barrier = [&](uint32_t mip, VkImageLayout from, VkImageLayout to, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkPipelineStageFlags2 dstStage) {
				VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
				b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				b.srcAccessMask = srcAccess;
				b.dstStageMask = dstStage;
				b.dstAccessMask = dstAccess;
				b.oldLayout = from;
				b.newLayout = to;
				b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.image = dst->GetImage();
				b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, 0, 1 };
				VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
				dep.imageMemoryBarrierCount = 1;
				dep.pImageMemoryBarriers = &b;
				vkCmdPipelineBarrier2(cmd, &dep);
			};
			int32_t w = static_cast<int32_t>(t.Width), h = static_cast<int32_t>(t.Height);
			for (uint32_t m = 1; m < mips; m++) {
				barrier(m - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
				VkImageBlit mb{};
				mb.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, m - 1, 0, 1 };
				mb.srcOffsets[1] = { w, h, 1 };
				w = std::max(1, w / 2); h = std::max(1, h / 2);
				mb.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, m, 0, 1 };
				mb.dstOffsets[1] = { w, h, 1 };
				vkCmdBlitImage(cmd, dst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &mb, VK_FILTER_LINEAR);
				barrier(m - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
			}
			barrier(mips - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
			dst->SetTrackedLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		// Records the captured Renderer2D batches of one layer
		void Draw2D(VkCommandBuffer cmd, FrameResources& fr, const std::vector<Vulkan2DBatch>& batches, const std::vector<VkDeviceSize>& offsets,
			bool overlay, VulkanSceneRenderer::Statistics& stats) {
			auto& r = R();
			const VkDevice dev = VulkanContext::Get().GetDevice();
			bool indicesBound = false;
			for (size_t bi = 0; bi < batches.size(); bi++) {
				const Vulkan2DBatch& b = batches[bi];
				if ((b.DrawLayer == Vulkan2DBatch::Layer::Overlay) != overlay) continue;
				VkDeviceSize offset = offsets[bi];
				vkCmdPushConstants(cmd, r.Layout2D, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &b.ViewProjection);
				if (b.QuadCount) {
					// Textures of this batch (slot 0 and missing ones: white)
					VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
					alloc.descriptorPool = fr.Pool;
					alloc.descriptorSetCount = 1;
					alloc.pSetLayouts = &r.Set2DLayout;
					VkDescriptorSet set = VK_NULL_HANDLE;
					if (vkAllocateDescriptorSets(dev, &alloc, &set) == VK_SUCCESS) {
						VkDescriptorImageInfo images[32];
						for (uint32_t i = 0; i < 32; i++) {
							VulkanTexture2D* t = i < b.Textures.size() ? VkTex(b.Textures[i]) : nullptr;
							if (!t) t = r.White.get();
							images[i] = { t->GetDefaultSampler(), t->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
						}
						VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 0, 0, 32, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, images, nullptr, nullptr };
						vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
						vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Get2DPipeline(0, overlay, b.Additive, b.DepthTest, b.DepthWrite));
						vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout2D, 0, 1, &set, 0, nullptr);
						if (!indicesBound) { vkCmdBindIndexBuffer(cmd, r.QuadIndices.Handle, 0, VK_INDEX_TYPE_UINT32); indicesBound = true; }
						vkCmdBindVertexBuffers(cmd, 0, 1, &fr.Vertices2D.Buf.Handle, &offset);
						vkCmdDrawIndexed(cmd, std::min(b.QuadCount, kMaxQuads2D) * 6, 1, 0, 0, 0);
						stats.DrawCalls++;
					}
					offset += b.Quads.size();
				}
				if (b.CircleCount) {
					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Get2DPipeline(1, overlay, b.Additive, b.DepthTest, b.DepthWrite));
					if (!indicesBound) { vkCmdBindIndexBuffer(cmd, r.QuadIndices.Handle, 0, VK_INDEX_TYPE_UINT32); indicesBound = true; }
					vkCmdBindVertexBuffers(cmd, 0, 1, &fr.Vertices2D.Buf.Handle, &offset);
					vkCmdDrawIndexed(cmd, std::min(b.CircleCount, kMaxQuads2D) * 6, 1, 0, 0, 0);
					stats.DrawCalls++;
					offset += b.Circles.size();
				}
				if (b.LineVertexCount) {
					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Get2DPipeline(2, overlay, b.Additive, b.DepthTest, b.DepthWrite));
					if (r.WideLines) vkCmdSetLineWidth(cmd, std::clamp(b.LineWidth, 1.0f, 8.0f));
					vkCmdBindVertexBuffers(cmd, 0, 1, &fr.Vertices2D.Buf.Handle, &offset);
					vkCmdDraw(cmd, b.LineVertexCount, 1, 0, 0);
					stats.DrawCalls++;
				}
			}
		}

	}

	// ============================================================================
	// PUBLIC
	// ============================================================================

	VulkanSceneRenderer& VulkanSceneRenderer::Get() {
		static VulkanSceneRenderer& s = *new VulkanSceneRenderer();
		return s;
	}

	uint32_t VulkanSceneRenderer::GetOutputTextureID(uint32_t viewID) const {
		if (viewID >= kMaxViews) return 0;
		const auto& t = R().Views[viewID];
		return t.Output ? t.Output->GetRendererID() : 0;
	}

	void VulkanSceneRenderer::Submit2D(Vulkan2DBatch&& batch) {
		if (batch.QuadCount || batch.CircleCount || batch.LineVertexCount) m_2DBatches.push_back(std::move(batch));
	}

	void VulkanSceneRenderer::Clear2D() { m_2DBatches.clear(); }

	void VulkanSceneRenderer::Render(Scene& scene, const VulkanRenderView& view) {
		auto& r = R();
		auto& ctx = VulkanContext::Get();
		if (!ctx.IsFrameActive() || r.Failed || view.Width == 0 || view.Height == 0 || view.ViewID >= kMaxViews) return;
		if (!r.Initialized) {
			r.Initialized = true;
			if (!Init()) { r.Failed = true; LNX_LOG_ERROR("Vulkan scene renderer could not start"); return; }
		}
		const auto t0 = std::chrono::high_resolution_clock::now();
		const bool mainView = view.ViewID == 0;
		Statistics stats;
		r.FrameCounter++;
		const VkDevice dev = ctx.GetDevice();
		ViewTargets& vt = r.Views[view.ViewID];
		FrameResources& fr = vt.Frames[ctx.GetFrameIndex()];
		const uint32_t W = view.Width, H = view.Height;

		// GPU time of the previous use of this slot (its fence was waited in BeginFrame)
		const uint32_t firstQuery = 2 * (view.ViewID * kFramesInFlight + ctx.GetFrameIndex());
		if (fr.TimestampsPending && r.Timestamps) {
			uint64_t ticks[2] = {};
			if (vkGetQueryPoolResults(dev, r.Timestamps, firstQuery, 2, sizeof(ticks), ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
				stats.GpuMs = static_cast<float>(ticks[1] - ticks[0]) * ctx.GetProperties().limits.timestampPeriod * 1e-6f;
			fr.TimestampsPending = false;
		}
		// Picking result of the previous use of this slot
		if (fr.ReadbackPending) {
			if (mainView) std::memcpy(&m_PickedEntity, fr.Readback.Memory.Mapped, sizeof(int));
			fr.ReadbackPending = false;
		}

		EnsureTargets(vt, W, H);
		UpdateEnvironment();
		vkResetDescriptorPool(dev, fr.Pool, 0);

		// ---- Gather lights ----
		std::vector<GPULight> lights;
		int sunIndex = -1;
		bool sunIsFlagged = false;
		{
			auto lightView = scene.GetAllEntitiesWith<TransformComponent, LightComponent>();
			for (auto e : lightView) {
				Entity entity{ e, &scene };
				auto& lc = entity.GetComponent<LightComponent>();
				if (!lc.LightInstance) continue;
				const glm::mat4 world = scene.GetWorldTransform(entity);
				GPULight l;
				const int type = static_cast<int>(lc.GetType());
				l.PosRange = glm::vec4(glm::vec3(world[3]), lc.GetRange());
				l.DirType = glm::vec4(-glm::normalize(glm::vec3(world[2])), static_cast<float>(type));
				l.Color = glm::vec4(lc.GetColor(), lc.GetIntensity());
				l.Cone = glm::vec4(std::cos(glm::radians(lc.GetInnerConeAngle())), std::cos(glm::radians(lc.GetOuterConeAngle())), 0.0f, 0.0f);
				// Shadowed sun: the light flagged as sun, else the first shadow-casting directional light
				if (type == 0 && lc.GetCastShadows() && (sunIndex < 0 || (lc.IsSunLight() && !sunIsFlagged))) {
					sunIndex = static_cast<int>(lights.size());
					sunIsFlagged = lc.IsSunLight();
				}
				lights.push_back(l);
				if (lights.size() >= 1024) break;
			}
		}
		stats.Lights = static_cast<uint32_t>(lights.size());

		// ---- Gather draws (frustum culled) ----
		const glm::mat4 viewProj = view.Projection * view.View;
		const ViewFrustum frustum = ViewFrustum::FromViewProjection(viewProj);
		std::vector<GPUMaterial> materials;
		std::vector<glm::mat4> bones;
		std::vector<DrawItem> draws, transparent, casters;
		std::vector<uint8_t> graphParams;
		materials.push_back({ glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), glm::vec4(0, 0, 0, 1), glm::vec4(0.0f, 0.5f, 1.0f, 0.5f), {}, {}, glm::vec4(1, 1, 0, 0), glm::vec4(0, 0, 0, 1), glm::vec4(0.0f) });
		const VkDescriptorSet defaultSet = MaterialSet(nullptr, 0, true);

		// Graph material of an asset (params appended to this frame's params buffer)
		std::vector<glm::vec4> paramScratch;
		std::vector<Ref<Texture2D>> texScratch;
		auto bindGraph = [&](const Ref<MaterialAsset>& asset, const MaterialInstance* instance, bool skinned) -> GraphBinding {
			GraphBinding gb;
			MaterialGraph::GraphRuntime* runtime = asset ? asset->GetGraphRuntime() : nullptr;
			if (!runtime || !runtime->GetCompiled().IsValid() || !asset->GetNodeGraph()) return gb;
			gb.Forward = runtime->IsForward();
			gb.Pipeline = GetGraphPipeline(runtime->GetCompiled(), gb.Forward, skinned);
			if (!gb.Pipeline) return gb;
			runtime->GatherData(*asset->GetNodeGraph(), instance ? &instance->GetGraphParameters() : nullptr, paramScratch, texScratch);
			const size_t aligned = (graphParams.size() + r.StorageAlign - 1) / r.StorageAlign * r.StorageAlign;
			graphParams.resize(aligned + paramScratch.size() * sizeof(glm::vec4));
			std::memcpy(graphParams.data() + aligned, paramScratch.data(), paramScratch.size() * sizeof(glm::vec4));
			gb.ParamsOffset = static_cast<uint32_t>(aligned);
			VulkanTexture2D* tex[MaterialGraph::kMaxGraphTextures] = {};
			for (size_t i = 0; i < texScratch.size() && i < MaterialGraph::kMaxGraphTextures; i++) tex[i] = VkTex(texScratch[i]);
			gb.Textures = MaterialSet(tex, MaterialGraph::kMaxGraphTextures, false);
			return gb;
		};

		auto meshView = scene.GetAllEntitiesWith<TransformComponent, MeshComponent>();
		for (auto e : meshView) {
			Entity entity{ e, &scene };
			auto& mc = entity.GetComponent<MeshComponent>();
			if (!mc.MeshModel) continue;
			const glm::mat4 world = scene.GetWorldTransform(entity);
			int boneOffset = 0;
			bool hasBones = false;
			if (AnimatorComponent* animator = entity.HasComponent<AnimatorComponent>() ? &entity.GetComponent<AnimatorComponent>() : nullptr; animator && !animator->BoneMatrices.empty()) {
				boneOffset = static_cast<int>(bones.size());
				bones.insert(bones.end(), animator->BoneMatrices.begin(), animator->BoneMatrices.end());
				hasBones = true;
			}

			// Entity material (or per-submesh imported materials while the default one is assigned)
			MaterialComponent* matComp = entity.HasComponent<MaterialComponent>() ? &entity.GetComponent<MaterialComponent>() : nullptr;
			const bool perSubmesh = mc.HasPerSubmeshMaterials() && (!matComp || matComp->IsDefaultMaterial());
			int entityMaterial = 0;
			VkDescriptorSet entitySet = defaultSet;
			Ref<MaterialAsset> entityAsset;
			if (matComp && matComp->Instance && !perSubmesh) {
				GPUMaterial gm{};
				entityAsset = matComp->Instance->GetBaseAsset();
				entitySet = FillMaterial(gm, matComp->Instance->GetUniformData(), entityAsset);
				entityMaterial = static_cast<int>(materials.size());
				materials.push_back(gm);
			}
			GraphBinding entityGraph[2];   // static / skinned

			const auto& meshes = mc.MeshModel->GetMeshes();
			for (size_t i = 0; i < meshes.size(); i++) {
				const Ref<Mesh>& mesh = meshes[i];
				const GPUMesh* g = GetMesh(*mesh);
				if (!g) continue;
				DrawItem d;
				d.Mesh = g;
				d.Model = world;
				d.Entity = static_cast<int>(e);
				d.Skinned = hasBones && mesh->IsSkinned() && g->Bones.Handle;
				d.BoneOffset = boneOffset;
				// Off-screen meshes still cast shadows into the view
				if (sunIndex >= 0 && view.Shadows) casters.push_back(d);
				glm::vec3 mn, mx;
				Mesh::TransformAABB(world, mesh->GetAABBMin(), mesh->GetAABBMax(), mn, mx);
				if (!mesh->IsSkinned() && !frustum.IntersectsAABB(mn, mx)) { stats.Culled++; continue; }
				d.Material = entityMaterial;
				d.Textures = entitySet;
				GraphBinding gb;
				if (perSubmesh) {
					if (Ref<MaterialAsset> sub = mc.GetSubmeshMaterial(static_cast<uint32_t>(i))) {
						gb = bindGraph(sub, nullptr, d.Skinned);
						if (!gb.Pipeline) {
							GPUMaterial gm{};
							d.Textures = FillMaterial(gm, sub->GetUniformData(), sub);
							d.Material = static_cast<int>(materials.size());
							materials.push_back(gm);
						}
					}
				}
				else if (entityAsset && entityAsset->UsesNodeGraph()) {
					GraphBinding& cached = entityGraph[d.Skinned ? 1 : 0];
					if (!cached.Pipeline) cached = bindGraph(entityAsset, matComp->Instance.get(), d.Skinned);
					gb = cached;
				}
				if (gb.Pipeline) {
					d.Pipeline = gb.Pipeline;
					d.Textures = gb.Textures;
					d.ParamsOffset = gb.ParamsOffset;
					stats.GraphDraws++;
				}
				if (gb.Pipeline && gb.Forward) {
					d.Distance = glm::length((mn + mx) * 0.5f - view.CameraPosition);
					transparent.push_back(d);
				}
				else draws.push_back(d);
			}
		}
		// Fewer state changes: group by pipeline, then by texture set
		std::sort(draws.begin(), draws.end(), [](const DrawItem& a, const DrawItem& b) {
			if (a.Pipeline != b.Pipeline) return a.Pipeline < b.Pipeline;
			if (a.Skinned != b.Skinned) return a.Skinned < b.Skinned;
			return a.Textures < b.Textures;
		});
		std::sort(transparent.begin(), transparent.end(), [](const DrawItem& a, const DrawItem& b) { return a.Distance > b.Distance; });
		std::sort(casters.begin(), casters.end(), [](const DrawItem& a, const DrawItem& b) { return a.Skinned < b.Skinned; });
		stats.TransparentDraws = static_cast<uint32_t>(transparent.size());

		// ---- Sun shadow matrix: ortho box around the first kShadowDistance meters of the view ----
		glm::mat4 lightViewProj(1.0f);
		float worldTexel = 0.0f;
		const bool shadows = sunIndex >= 0 && view.Shadows && !casters.empty();
		if (shadows) {
			constexpr float kShadowDistance = 60.0f;
			const glm::mat4 invVP = glm::inverse(viewProj);
			glm::vec3 corners[8];
			int c = 0;
			for (int y = -1; y <= 1; y += 2)
				for (int x = -1; x <= 1; x += 2) {
					glm::vec4 n = invVP * glm::vec4(x, y, -1.0f, 1.0f);
					glm::vec4 f = invVP * glm::vec4(x, y, 1.0f, 1.0f);
					const glm::vec3 np = glm::vec3(n) / n.w, fp = glm::vec3(f) / f.w;
					const float len = glm::length(fp - np);
					corners[c++] = np;
					corners[c++] = np + (fp - np) * std::min(1.0f, kShadowDistance / std::max(len, 1e-4f));
				}
			glm::vec3 center(0.0f);
			for (const auto& p : corners) center += p;
			center /= 8.0f;
			float radius = 0.0f;
			for (const auto& p : corners) radius = std::max(radius, glm::length(p - center));
			radius = std::ceil(radius * 2.0f) * 0.5f;   // quantized: stable texel size while the camera moves
			const glm::vec3 dir = glm::normalize(glm::vec3(lights[sunIndex].DirType));
			const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
			constexpr float kBackDistance = 200.0f;      // casters between the sun and the view box
			const glm::mat4 lightView = glm::lookAt(center - dir * (radius + kBackDistance), center, up);
			glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 0.0f, 2.0f * radius + kBackDistance);
			// Snap to whole texels: no shimmering when the camera moves
			glm::vec4 origin = lightProj * lightView * glm::vec4(0, 0, 0, 1);
			origin *= static_cast<float>(kShadowSize) * 0.5f;
			const glm::vec4 offset = (glm::round(origin) - origin) * (2.0f / static_cast<float>(kShadowSize));
			lightProj[3][0] += offset.x;
			lightProj[3][1] += offset.y;
			lightViewProj = lightProj * lightView;
			worldTexel = 2.0f * radius / static_cast<float>(kShadowSize);
		}

		// ---- Renderer2D batches captured this frame ----
		std::vector<VkDeviceSize> offsets2D;
		std::vector<uint8_t> vertices2D;
		bool scene2D = false, overlay2D = false;
		if (view.Draw2D) {
			for (const Vulkan2DBatch& b : m_2DBatches) {
				offsets2D.push_back(vertices2D.size());
				vertices2D.insert(vertices2D.end(), b.Quads.begin(), b.Quads.end());
				vertices2D.insert(vertices2D.end(), b.Circles.begin(), b.Circles.end());
				vertices2D.insert(vertices2D.end(), b.Lines.begin(), b.Lines.end());
				(b.DrawLayer == Vulkan2DBatch::Layer::Overlay ? overlay2D : scene2D) = true;
			}
			stats.Batches2D = static_cast<uint32_t>(m_2DBatches.size());
		}

		// ---- Upload frame data ----
		FrameUBO frame{};
		frame.View = view.View;
		frame.Proj = view.Projection;
		frame.ViewProj = viewProj;
		frame.InvViewProj = glm::inverse(viewProj);
		frame.CamPos = glm::vec4(view.CameraPosition, 1.0f);
		Ref<EnvironmentMap> envMap = SkyboxRenderer::GetGlobalEnvironment();
		frame.EnvParams = glm::vec4(envMap ? envMap->GetIntensity() : 1.0f, envMap ? glm::radians(envMap->GetRotation()) : 0.0f, r.Env ? 1.0f : 0.0f, r.EnvMips);
		frame.Counts = glm::ivec4(static_cast<int>(lights.size()), 0, shadows ? sunIndex : -1, 0);
		frame.LightViewProj = lightViewProj;
		frame.ShadowParams = glm::vec4(shadows ? 1.0f : 0.0f, 1.0f / static_cast<float>(kShadowSize), 1.0f, worldTexel);
		frame.OutlineColor = glm::vec4(1.0f, 0.55f, 0.1f, 1.0f);
		frame.Viewport = glm::vec4(static_cast<float>(W), static_cast<float>(H), 1.0f / W, 1.0f / H);
		if (bones.empty()) bones.push_back(glm::mat4(1.0f));
		if (lights.empty()) lights.push_back({});
		graphParams.resize(graphParams.size() + MaterialGraph::kMaxGraphParams * sizeof(glm::vec4));   // tail: whole binding range valid
		if (vertices2D.empty()) vertices2D.resize(64);

		fr.Frame.Ensure(sizeof(FrameUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		fr.Lights.Ensure(lights.size() * sizeof(GPULight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		fr.Bones.Ensure(bones.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		fr.Materials.Ensure(materials.size() * sizeof(GPUMaterial), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		fr.GraphParams.Ensure(graphParams.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		fr.Vertices2D.Ensure(vertices2D.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		std::memcpy(fr.Frame.Buf.Memory.Mapped, &frame, sizeof(frame));
		std::memcpy(fr.Lights.Buf.Memory.Mapped, lights.data(), lights.size() * sizeof(GPULight));
		std::memcpy(fr.Bones.Buf.Memory.Mapped, bones.data(), bones.size() * sizeof(glm::mat4));
		std::memcpy(fr.Materials.Buf.Memory.Mapped, materials.data(), materials.size() * sizeof(GPUMaterial));
		std::memcpy(fr.GraphParams.Buf.Memory.Mapped, graphParams.data(), graphParams.size());
		std::memcpy(fr.Vertices2D.Buf.Memory.Mapped, vertices2D.data(), vertices2D.size());

		// Sets 0 and 2 for this frame (buffers may have been reallocated)
		const bool needSceneColor = !transparent.empty();
		Ref<VulkanTexture2D> sceneColor = needSceneColor ? GetSceneColor(vt) : nullptr;
		{
			const VkDescriptorSetLayout layouts[2] = { r.Set0Layout, r.GraphSetLayout };
			VkDescriptorSet sets[2] = {};
			VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			alloc.descriptorPool = fr.Pool;
			alloc.descriptorSetCount = 2;
			alloc.pSetLayouts = layouts;
			vkAllocateDescriptorSets(dev, &alloc, sets);
			fr.Set0 = sets[0];
			fr.GraphSet = sets[1];
			VkDescriptorBufferInfo bufs[5] = {
				{ fr.Frame.Buf.Handle, 0, sizeof(FrameUBO) },
				{ fr.Lights.Buf.Handle, 0, VK_WHOLE_SIZE },
				{ fr.Bones.Buf.Handle, 0, VK_WHOLE_SIZE },
				{ fr.Materials.Buf.Handle, 0, VK_WHOLE_SIZE },
				{ fr.GraphParams.Buf.Handle, 0, MaterialGraph::kMaxGraphParams * sizeof(glm::vec4) },
			};
			VulkanTexture2D* envTex = r.Env ? r.Env.get() : r.Black.get();
			VkDescriptorImageInfo envInfo{ ctx.GetSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT), envTex->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkDescriptorImageInfo shadowInfo{ ctx.GetSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true),
				r.Shadow->GetView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
			VulkanTexture2D* sceneTex = sceneColor ? sceneColor.get() : r.Black.get();
			VkDescriptorImageInfo sceneInfo{ ctx.GetSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
				sceneTex->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkWriteDescriptorSet writes[8] = {
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufs[0], nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufs[1], nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufs[2], nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufs[3], nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &envInfo, nullptr, nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 5, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowInfo, nullptr, nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 6, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, nullptr, nullptr },
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.GraphSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, nullptr, &bufs[4], nullptr },
			};
			vkUpdateDescriptorSets(dev, 8, writes, 0, nullptr);
		}

		// ---- Record ----
		VkCommandBuffer cmd = ctx.GetCommandBuffer();
		if (r.Timestamps) {
			vkCmdResetQueryPool(cmd, r.Timestamps, firstQuery, 2);
			vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_NONE, r.Timestamps, firstQuery);
		}

		auto drawMeshes = [&](const std::vector<DrawItem>& items) {
			VkPipeline bound = VK_NULL_HANDLE;
			VkDescriptorSet boundSet = VK_NULL_HANDLE;
			for (const DrawItem& d : items) {
				const VkPipeline pipeline = d.Pipeline ? d.Pipeline : (d.Skinned ? r.SkinnedPipe : r.MeshPipe);
				if (pipeline != bound) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); bound = pipeline; }
				if (d.Textures != boundSet) { vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 1, 1, &d.Textures, 0, nullptr); boundSet = d.Textures; }
				if (d.Pipeline) vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 2, 1, &fr.GraphSet, 1, &d.ParamsOffset);
				PushData push{ d.Model, d.Material, d.Entity, d.BoneOffset, d.Skinned ? 1 : 0 };
				vkCmdPushConstants(cmd, r.Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
				VkBuffer vbs[2] = { d.Mesh->Vertices.Handle, d.Mesh->Bones.Handle };
				VkDeviceSize offsets[2] = { 0, 0 };
				vkCmdBindVertexBuffers(cmd, 0, d.Skinned ? 2 : 1, vbs, offsets);
				vkCmdBindIndexBuffer(cmd, d.Mesh->Indices.Handle, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, d.Mesh->IndexCount, 1, 0, 0, 0);
				stats.DrawCalls++;
				stats.Triangles += d.Mesh->IndexCount / 3;
			}
		};

		// 1. Sun shadow map (depth only)
		if (shadows) {
			r.Shadow->Transition(cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
			VkRenderingAttachmentInfo depth = Attachment(r.Shadow.get(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true);
			depth.clearValue.depthStencil = { 1.0f, 0 };
			BeginPass(cmd, kShadowSize, kShadowSize, nullptr, 0, &depth);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 0, 1, &fr.Set0, 0, nullptr);
			VkPipeline bound = VK_NULL_HANDLE;
			for (const DrawItem& d : casters) {
				const VkPipeline pipeline = d.Skinned ? r.ShadowSkinnedPipe : r.ShadowPipe;
				if (pipeline != bound) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); bound = pipeline; }
				PushData push{ d.Model, 0, d.Entity, d.BoneOffset, d.Skinned ? 1 : 0 };
				vkCmdPushConstants(cmd, r.Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
				VkBuffer vbs[2] = { d.Mesh->Vertices.Handle, d.Mesh->Bones.Handle };
				VkDeviceSize offsets[2] = { 0, 0 };
				vkCmdBindVertexBuffers(cmd, 0, d.Skinned ? 2 : 1, vbs, offsets);
				vkCmdBindIndexBuffer(cmd, d.Mesh->Indices.Handle, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, d.Mesh->IndexCount, 1, 0, 0, 0);
				stats.ShadowDrawCalls++;
			}
			vkCmdEndRendering(cmd);
		}
		r.Shadow->Transition(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

		// 2. Opaque HDR pass
		vt.HDR->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vt.Entity->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vt.Depth->Transition(cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		VkRenderingAttachmentInfo hdrColors[2] = {
			Attachment(vt.HDR.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true),
			Attachment(vt.Entity.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true),
		};
		hdrColors[0].clearValue.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
		hdrColors[1].clearValue.color.int32[0] = -1;
		VkRenderingAttachmentInfo depth = Attachment(vt.Depth.get(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true);
		depth.clearValue.depthStencil = { 1.0f, 0 };
		BeginPass(cmd, W, H, hdrColors, 2, &depth);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 0, 1, &fr.Set0, 0, nullptr);
		drawMeshes(draws);
		stats.Meshes = static_cast<uint32_t>(draws.size() + transparent.size());
		if (view.DrawSky) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.SkyPipe); vkCmdDraw(cmd, 3, 1, 0, 0); }
		vkCmdEndRendering(cmd);

		// 3. Transparent HDR pass: refraction source, sorted glass / blended graph materials, 2D scene batches
		if (needSceneColor || scene2D) {
			if (needSceneColor) CopySceneColor(cmd, vt);
			vt.HDR->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			hdrColors[0].loadOp = hdrColors[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			BeginPass(cmd, W, H, hdrColors, 2, &depth);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 0, 1, &fr.Set0, 0, nullptr);
			drawMeshes(transparent);
			if (scene2D) Draw2D(cmd, fr, m_2DBatches, offsets2D, false, stats);
			vkCmdEndRendering(cmd);
		}
		vt.HDR->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// 4. Bloom
		const PostProcessConfig& pp = PostProcessRenderer::GetConfig();
		const bool bloom = view.PostProcess && pp.EnableBloom && W >= 16 && H >= 16;
		if (bloom) {
			const int levels = std::clamp(pp.BloomMipLevels, 2, 8);
			EnsureBloom(vt, levels);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.BloomDownPipe);
			for (int i = 0; i < levels; i++) {
				VulkanTexture2D* src = i == 0 ? vt.HDR.get() : vt.Bloom[i - 1].get();
				VulkanTexture2D* dst = vt.Bloom[i].get();
				dst->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				VkRenderingAttachmentInfo a = Attachment(dst, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
				a.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				BeginPass(cmd, dst->GetWidth(), dst->GetHeight(), &a, 1, nullptr);
				VkDescriptorSet set = PostSet(fr, src, nullptr);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.PostLayout, 0, 1, &set, 0, nullptr);
				PostPush push{ glm::vec4(1.0f / src->GetWidth(), 1.0f / src->GetHeight(), pp.BloomThreshold, 0.0f), {}, {}, glm::ivec4(i == 0 ? 1 : 0, 0, 0, 0) };
				vkCmdPushConstants(cmd, r.PostLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
				vkCmdDraw(cmd, 3, 1, 0, 0);
				vkCmdEndRendering(cmd);
				dst->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.BloomUpPipe);
			for (int i = levels - 1; i > 0; i--) {
				VulkanTexture2D* src = vt.Bloom[i].get();
				VulkanTexture2D* dst = vt.Bloom[i - 1].get();
				dst->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				VkRenderingAttachmentInfo a = Attachment(dst, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
				BeginPass(cmd, dst->GetWidth(), dst->GetHeight(), &a, 1, nullptr);
				VkDescriptorSet set = PostSet(fr, src, nullptr);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.PostLayout, 0, 1, &set, 0, nullptr);
				PostPush push{ glm::vec4(1.0f / src->GetWidth(), 1.0f / src->GetHeight(), std::max(pp.BloomRadius, 0.1f), 0.0f), {}, {}, glm::ivec4(0) };
				vkCmdPushConstants(cmd, r.PostLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
				vkCmdDraw(cmd, 3, 1, 0, 0);
				vkCmdEndRendering(cmd);
				dst->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		}

		// Selection mask (selected meshes and children, no depth test)
		bool anySelected = false;
		if (!view.Selected.empty())
			for (const std::vector<DrawItem>* list : { &draws, &transparent })
				for (const DrawItem& d : *list)
					if (std::find(view.Selected.begin(), view.Selected.end(), d.Entity) != view.Selected.end()) { anySelected = true; break; }
		if (anySelected) {
			vt.Mask->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			VkRenderingAttachmentInfo maskTarget = Attachment(vt.Mask.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
			BeginPass(cmd, W, H, &maskTarget, 1, nullptr);
			VkPipeline bound = VK_NULL_HANDLE;
			for (const std::vector<DrawItem>* list : { &draws, &transparent })
				for (const DrawItem& d : *list) {
					if (std::find(view.Selected.begin(), view.Selected.end(), d.Entity) == view.Selected.end()) continue;
					const VkPipeline pipeline = d.Skinned ? r.MaskSkinnedPipe : r.MaskPipe;
					if (pipeline != bound) {
						vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
						vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 0, 1, &fr.Set0, 0, nullptr);
						bound = pipeline;
					}
					PushData push{ d.Model, 0, d.Entity, d.BoneOffset, d.Skinned ? 1 : 0 };
					vkCmdPushConstants(cmd, r.Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
					VkBuffer vbs[2] = { d.Mesh->Vertices.Handle, d.Mesh->Bones.Handle };
					VkDeviceSize offsets[2] = { 0, 0 };
					vkCmdBindVertexBuffers(cmd, 0, d.Skinned ? 2 : 1, vbs, offsets);
					vkCmdBindIndexBuffer(cmd, d.Mesh->Indices.Handle, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(cmd, d.Mesh->IndexCount, 1, 0, 0, 0);
				}
			vkCmdEndRendering(cmd);
			vt.Mask->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		// 5. Composite to 8 bit + grid + overlays + outline
		vt.Output->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkRenderingAttachmentInfo ldrColors[2] = {
			Attachment(vt.Output.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false),
			Attachment(vt.Entity.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false),
		};
		ldrColors[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		BeginPass(cmd, W, H, ldrColors, 2, &depth);
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.CompositePipe);
			VkDescriptorSet set = PostSet(fr, vt.HDR.get(), bloom ? vt.Bloom[0].get() : nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.PostLayout, 0, 1, &set, 0, nullptr);
			const bool post = view.PostProcess;
			PostPush push{
				glm::vec4(pp.BloomIntensity, post ? pp.Exposure : 1.0f, post ? pp.Gamma : 2.2f, pp.ChromaticAberrationIntensity),
				glm::vec4(pp.VignetteIntensity, pp.VignetteRoundness, pp.VignetteSmoothness, 0.0f), {},
				glm::ivec4(bloom ? 1 : 0, post && pp.EnableVignette ? 1 : 0, post && pp.EnableChromaticAberration ? 1 : 0, post ? pp.ToneMapOperator : 0) };
			vkCmdPushConstants(cmd, r.PostLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
			vkCmdDraw(cmd, 3, 1, 0, 0);
		}
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 0, 1, &fr.Set0, 0, nullptr);
		if (view.DrawGrid) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.GridPipe); vkCmdDraw(cmd, 3, 1, 0, 0); }
		if (overlay2D) Draw2D(cmd, fr, m_2DBatches, offsets2D, true, stats);
		if (anySelected) {
			VulkanTexture2D* maskTex[1] = { vt.Mask.get() };
			const VkDescriptorSet maskSet = MaterialSet(maskTex, 1, false);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.OutlinePipe);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 0, 1, &fr.Set0, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 1, 1, &maskSet, 0, nullptr);
			vkCmdDraw(cmd, 3, 1, 0, 0);
		}
		vkCmdEndRendering(cmd);

		// Entity under the mouse (read when this slot comes back)
		if (mainView) {
			if (view.PickX >= 0 && view.PickY >= 0 && view.PickX < static_cast<int>(W) && view.PickY < static_cast<int>(H)) {
				vt.Entity->Transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				VkBufferImageCopy copy{};
				copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
				// GL-order rows: row 0 is the bottom of the view, like the pick coordinates
				copy.imageOffset = { view.PickX, view.PickY, 0 };
				copy.imageExtent = { 1, 1, 1 };
				vkCmdCopyImageToBuffer(cmd, vt.Entity->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fr.Readback.Handle, 1, &copy);
				fr.ReadbackPending = true;
			}
			else m_PickedEntity = -1;
		}
		vt.Output->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vt.Entity->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vt.Depth->Transition(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
		if (r.Timestamps) {
			vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, r.Timestamps, firstQuery + 1);
			fr.TimestampsPending = true;
		}
		if (view.Draw2D) m_2DBatches.clear();

		// Free geometry nobody drew for a while
		if ((r.FrameCounter & 255) == 0) {
			for (auto it = r.Meshes.begin(); it != r.Meshes.end();) {
				if (r.FrameCounter - it->second.LastUsed > 600) {
					DestroyBuffer(it->second.Vertices); DestroyBuffer(it->second.Indices); DestroyBuffer(it->second.Bones);
					it = r.Meshes.erase(it);
				}
				else ++it;
			}
		}
		stats.CpuMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
		if (mainView) m_Stats = stats;
	}

}
