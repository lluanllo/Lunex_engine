#include "stpch.h"
#include "MaterialGraphCompiler.h"

#include "MaterialGraph.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBuffer.h"
#include "Log/Log.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace Lunex::MaterialGraph {

	using namespace Graph;

	// ============================================================================
	// GLSL LIBRARY (split in pieces: MSVC limits a single string literal to 16 KB)
	// ============================================================================

	static const char* kLibCore = R"GLSL(
// ---------------- Lunex material graph library ----------------
struct LnxBSDF {
	vec3 BaseColor; float Metallic; float Roughness; float Specular;
	vec3 Emission; vec3 Normal; float AO; float Alpha; vec4 Subsurface;
	float IOR; float Transmission; float Refract; vec3 SpecularTint;
	float Coat; float CoatRoughness; float CoatIOR; vec3 CoatTint;
	float Sheen; float SheenRoughness; vec3 SheenTint;
};

LnxBSDF lnx_bsdf_default() {
	LnxBSDF b;
	b.BaseColor = vec3(0.8); b.Metallic = 0.0; b.Roughness = 0.5; b.Specular = 0.5;
	b.Emission = vec3(0.0); b.Normal = LNX_NORMAL; b.AO = 1.0; b.Alpha = 1.0; b.Subsurface = vec4(0.0);
	b.IOR = 1.5; b.Transmission = 0.0; b.Refract = 1.0; b.SpecularTint = vec3(1.0);
	b.Coat = 0.0; b.CoatRoughness = 0.03; b.CoatIOR = 1.5; b.CoatTint = vec3(1.0);
	b.Sheen = 0.0; b.SheenRoughness = 0.5; b.SheenTint = vec3(1.0);
	return b;
}

LnxBSDF lnx_bsdf_mix(LnxBSDF a, LnxBSDF b, float t) {
	t = clamp(t, 0.0, 1.0);
	LnxBSDF r;
	r.BaseColor = mix(a.BaseColor, b.BaseColor, t);
	r.Metallic = mix(a.Metallic, b.Metallic, t);
	r.Roughness = mix(a.Roughness, b.Roughness, t);
	r.Specular = mix(a.Specular, b.Specular, t);
	r.Emission = mix(a.Emission, b.Emission, t);
	r.Normal = normalize(mix(a.Normal, b.Normal, t));
	r.AO = mix(a.AO, b.AO, t);
	r.Alpha = mix(a.Alpha, b.Alpha, t);
	r.Subsurface = mix(a.Subsurface, b.Subsurface, t);
	r.IOR = mix(a.IOR, b.IOR, t);
	r.Transmission = mix(a.Transmission, b.Transmission, t);
	r.Refract = mix(a.Refract, b.Refract, t);
	r.SpecularTint = mix(a.SpecularTint, b.SpecularTint, t);
	r.Coat = mix(a.Coat, b.Coat, t);
	r.CoatRoughness = mix(a.CoatRoughness, b.CoatRoughness, t);
	r.CoatIOR = mix(a.CoatIOR, b.CoatIOR, t);
	r.CoatTint = mix(a.CoatTint, b.CoatTint, t);
	r.Sheen = mix(a.Sheen, b.Sheen, t);
	r.SheenRoughness = mix(a.SheenRoughness, b.SheenRoughness, t);
	r.SheenTint = mix(a.SheenTint, b.SheenTint, t);
	return r;
}

LnxBSDF lnx_bsdf_add(LnxBSDF a, LnxBSDF b) {
	LnxBSDF r = lnx_bsdf_mix(a, b, 0.5);
	r.BaseColor = min(a.BaseColor + b.BaseColor, vec3(1.0));
	r.Emission = a.Emission + b.Emission;
	r.Alpha = max(a.Alpha, b.Alpha);
	r.Coat = min(a.Coat + b.Coat, 1.0);
	r.Sheen = min(a.Sheen + b.Sheen, 1.0);
	r.Transmission = min(a.Transmission + b.Transmission, 1.0);
	return r;
}

// Shader to RGB: lighting of a closure as a color (lnx_light is provided by the shader template)
vec4 lnx_shader_to_rgb(LnxBSDF b) {
	vec3 c = lnx_light(b.BaseColor * (1.0 - b.Transmission), b.Metallic, b.Roughness, b.Specular, normalize(b.Normal), b.AO) + b.Emission;
	return vec4(c, b.Alpha);
}

float lnx_luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float lnx_avg(vec3 v) { return (v.x + v.y + v.z) / 3.0; }
float lnx_avg(vec2 v) { return (v.x + v.y) * 0.5; }

vec3 lnx_rgb2hsv(vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 lnx_hsv2rgb(vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 lnx_view_dir() { return normalize(LNX_VIEWPOS - LNX_POS); }

float lnx_fresnel(float ior, vec3 N) {
	float f0 = (ior - 1.0) / (ior + 1.0);
	f0 *= f0;
	float c = clamp(dot(normalize(N), lnx_view_dir()), 0.0, 1.0);
	return f0 + (1.0 - f0) * pow(1.0 - c, 5.0);
}

float lnx_facing(float blend, vec3 N) {
	float d = abs(dot(normalize(N), lnx_view_dir()));
	blend = clamp(blend, 0.0, 0.99999);
	float e = blend < 0.5 ? 2.0 * blend : 0.5 / (1.0 - blend);
	return 1.0 - pow(d, e);
}

vec3 lnx_normal_map(vec4 color, float strength) {
	vec3 n = color.rgb * 2.0 - 1.0;
	n.xy *= strength;
	return normalize(LNX_TBN * normalize(n));
}

vec3 lnx_bump(float strength, float dist, float h, vec3 N, bool invert) {
	vec3 P = LNX_POS;
	vec3 dpdx = dFdx(P), dpdy = dFdy(P);
	float dhdx = dFdx(h), dhdy = dFdy(h);
	vec3 r1 = cross(dpdy, N), r2 = cross(N, dpdx);
	float det = dot(dpdx, r1);
	vec3 grad = sign(det) * (dhdx * r1 + dhdy * r2);
	float d = invert ? -dist : dist;
	vec3 bumped = normalize(abs(det) * N - d * grad);
	return normalize(mix(N, bumped, clamp(strength, 0.0, 1.0)));
}

mat3 lnx_euler(vec3 r) {
	float cx = cos(r.x), sx = sin(r.x), cy = cos(r.y), sy = sin(r.y), cz = cos(r.z), sz = sin(r.z);
	mat3 Rx = mat3(1.0, 0.0, 0.0, 0.0, cx, sx, 0.0, -sx, cx);
	mat3 Ry = mat3(cy, 0.0, -sy, 0.0, 1.0, 0.0, sy, 0.0, cy);
	mat3 Rz = mat3(cz, sz, 0.0, -sz, cz, 0.0, 0.0, 0.0, 1.0);
	return Rz * Ry * Rx;
}

vec3 lnx_safe_div3(vec3 a, vec3 b) {
	return vec3(b.x != 0.0 ? a.x / b.x : 0.0, b.y != 0.0 ? a.y / b.y : 0.0, b.z != 0.0 ? a.z / b.z : 0.0);
}

vec3 lnx_mapping(vec3 v, vec3 loc, vec3 rot, vec3 sc, int type) {
	mat3 R = lnx_euler(rot);
	if (type == 0) return R * (v * sc) + loc;
	if (type == 1) return lnx_safe_div3(transpose(R) * (v - loc), sc);
	if (type == 2) return R * (v * sc);
	return normalize(R * lnx_safe_div3(v, sc));
}
)GLSL";

	static const char* kLibNoise = R"GLSL(
vec3 lnx_hash33(vec3 p) {
	p = vec3(dot(p, vec3(127.1, 311.7, 74.7)), dot(p, vec3(269.5, 183.3, 246.1)), dot(p, vec3(113.5, 271.9, 124.6)));
	return fract(sin(p) * 43758.5453123);
}

float lnx_gnoise(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	float n000 = dot(lnx_hash33(i + vec3(0, 0, 0)) * 2.0 - 1.0, f - vec3(0, 0, 0));
	float n100 = dot(lnx_hash33(i + vec3(1, 0, 0)) * 2.0 - 1.0, f - vec3(1, 0, 0));
	float n010 = dot(lnx_hash33(i + vec3(0, 1, 0)) * 2.0 - 1.0, f - vec3(0, 1, 0));
	float n110 = dot(lnx_hash33(i + vec3(1, 1, 0)) * 2.0 - 1.0, f - vec3(1, 1, 0));
	float n001 = dot(lnx_hash33(i + vec3(0, 0, 1)) * 2.0 - 1.0, f - vec3(0, 0, 1));
	float n101 = dot(lnx_hash33(i + vec3(1, 0, 1)) * 2.0 - 1.0, f - vec3(1, 0, 1));
	float n011 = dot(lnx_hash33(i + vec3(0, 1, 1)) * 2.0 - 1.0, f - vec3(0, 1, 1));
	float n111 = dot(lnx_hash33(i + vec3(1, 1, 1)) * 2.0 - 1.0, f - vec3(1, 1, 1));
	return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
	           mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z) * 1.4;
}

float lnx_fbm(vec3 p, float detail, float roughness) {
	detail = clamp(detail, 0.0, 15.0);
	roughness = clamp(roughness, 0.0, 1.0);
	float fscale = 1.0, amp = 1.0, maxamp = 0.0, sum = 0.0;
	int n = int(detail);
	for (int i = 0; i <= 15; i++) {
		if (i > n) break;
		sum += lnx_gnoise(p * fscale) * amp;
		maxamp += amp;
		amp *= roughness;
		fscale *= 2.0;
	}
	float rmd = detail - floor(detail);
	if (rmd > 0.0) {
		float sum2 = sum + lnx_gnoise(p * fscale) * amp;
		return mix(sum / maxamp, sum2 / (maxamp + amp), rmd) * 0.5 + 0.5;
	}
	return (sum / maxamp) * 0.5 + 0.5;
}

vec4 lnx_noise_tex(vec3 p, float scale, float detail, float roughness, float distortion) {
	p *= scale;
	if (distortion != 0.0) {
		p += vec3(lnx_gnoise(p + vec3(13.5, 2.1, 7.7)), lnx_gnoise(p + vec3(-7.1, 3.3, 1.9)), lnx_gnoise(p + vec3(3.3, -9.2, 5.4))) * distortion;
	}
	float f = lnx_fbm(p, detail, roughness);
	vec3 c = vec3(f, lnx_fbm(p + vec3(5.2, 1.3, 7.7), detail, roughness), lnx_fbm(p + vec3(1.7, 9.2, 3.1), detail, roughness));
	return vec4(c, f);
}

void lnx_voronoi(vec3 p, float scale, float randomness, int feature, out float dist, out vec3 col, out vec3 pos) {
	p *= scale;
	vec3 cell = floor(p);
	vec3 local = p - cell;
	float d1 = 8.0, d2 = 8.0;
	vec3 best = vec3(0.0), bestCell = cell;
	for (int z = -1; z <= 1; z++)
	for (int y = -1; y <= 1; y++)
	for (int x = -1; x <= 1; x++) {
		vec3 off = vec3(float(x), float(y), float(z));
		vec3 pt = off + lnx_hash33(cell + off) * clamp(randomness, 0.0, 1.0);
		float d = distance(pt, local);
		if (d < d1) { d2 = d1; d1 = d; best = pt; bestCell = cell + off; }
		else if (d < d2) { d2 = d; }
	}
	dist = feature == 0 ? d1 : (feature == 1 ? d2 : (d2 - d1) * 0.5);
	col = lnx_hash33(bestCell + vec3(7.31, 1.17, 3.53));
	pos = (cell + best) / max(scale, 1e-5);
}

float lnx_wave(vec3 p, float scale, float distortion, float detail, float phase, int type, int profile) {
	p *= scale;
	float n = (type == 0) ? (p.x + p.y + p.z) * 10.0 : length(p) * 20.0;
	if (distortion != 0.0) n += distortion * (lnx_fbm(p, detail, 0.5) * 2.0 - 1.0);
	n += phase;
	if (profile == 0) return 0.5 + 0.5 * sin(n - 1.5707963);
	n /= 6.2831853;
	if (profile == 1) return n - floor(n);
	return abs(n - floor(n + 0.5)) * 2.0;
}

vec4 lnx_checker(vec3 p, vec4 c1, vec4 c2, float scale, out float fac) {
	p = (p * scale + 0.000001) * 0.999999;
	ivec3 ip = ivec3(floor(p));
	bool even = ((ip.x + ip.y + ip.z) & 1) == 0;
	fac = even ? 1.0 : 0.0;
	return even ? c1 : c2;
}

float lnx_gradient(vec3 p, int type) {
	float r;
	if (type == 0) r = p.x;
	else if (type == 1) { r = max(p.x, 0.0); r *= r; }
	else if (type == 2) { r = clamp(p.x, 0.0, 1.0); float t = r * r; r = 3.0 * t - 2.0 * t * r; }
	else if (type == 3) r = (p.x + p.y) * 0.5;
	else if (type == 4) r = max(0.999999 - length(p), 0.0);
	else r = atan(p.y, p.x) / 6.2831853 + 0.5;
	return clamp(r, 0.0, 1.0);
}
)GLSL";

	static const char* kLibMath = R"GLSL(
float lnx_safe_div(float a, float b) { return b != 0.0 ? a / b : 0.0; }
float lnx_smin(float a, float b, float c) {
	if (c != 0.0) { float h = max(c - abs(a - b), 0.0) / c; return min(a, b) - h * h * h * c * (1.0 / 6.0); }
	return min(a, b);
}

float lnx_math(int op, float a, float b, float c) {
	switch (op) {
		case 0: return a + b;
		case 1: return a - b;
		case 2: return a * b;
		case 3: return lnx_safe_div(a, b);
		case 4: return a * b + c;
		case 5: {
			if (a >= 0.0) return pow(a, b);
			float fb = floor(b);
			if (fb == b) return (mod(fb, 2.0) == 0.0) ? pow(-a, b) : -pow(-a, b);
			return 0.0;
		}
		case 6: return (a > 0.0 && b > 0.0 && b != 1.0) ? log(a) / log(b) : 0.0;
		case 7: return a > 0.0 ? sqrt(a) : 0.0;
		case 8: return a > 0.0 ? inversesqrt(a) : 0.0;
		case 9: return abs(a);
		case 10: return exp(a);
		case 11: return min(a, b);
		case 12: return max(a, b);
		case 13: return a < b ? 1.0 : 0.0;
		case 14: return a > b ? 1.0 : 0.0;
		case 15: return sign(a);
		case 16: return abs(a - b) <= max(c, 1e-5) ? 1.0 : 0.0;
		case 17: return lnx_smin(a, b, c);
		case 18: return -lnx_smin(-a, -b, c);
		case 19: return floor(a + 0.5);
		case 20: return floor(a);
		case 21: return ceil(a);
		case 22: return trunc(a);
		case 23: return a - floor(a);
		case 24: return b != 0.0 ? a - b * trunc(a / b) : 0.0;
		case 25: return b != 0.0 ? a - b * floor(a / b) : 0.0;
		case 26: { float range = b - c; return range != 0.0 ? a - range * floor((a - c) / range) : c; }
		case 27: return b != 0.0 ? floor(a / b) * b : 0.0;
		case 28: return b != 0.0 ? abs(fract((a - b) / (b * 2.0)) * b * 2.0 - b) : 0.0;
		case 29: return sin(a);
		case 30: return cos(a);
		case 31: return tan(a);
		case 32: return asin(clamp(a, -1.0, 1.0));
		case 33: return acos(clamp(a, -1.0, 1.0));
		case 34: return atan(a);
		case 35: return atan(a, b);
		case 36: return sinh(a);
		case 37: return cosh(a);
		case 38: return tanh(a);
		case 39: return radians(a);
		case 40: return degrees(a);
	}
	return 0.0;
}

vec3 lnx_vmath(int op, vec3 a, vec3 b, vec3 c, float s) {
	switch (op) {
		case 0: return a + b;
		case 1: return a - b;
		case 2: return a * b;
		case 3: return lnx_safe_div3(a, b);
		case 4: return a * b + c;
		case 5: return cross(a, b);
		case 6: { float l = dot(b, b); return l != 0.0 ? b * (dot(a, b) / l) : vec3(0.0); }
		case 7: return reflect(a, normalize(b));
		case 11: return a * s;
		case 12: return length(a) != 0.0 ? normalize(a) : vec3(0.0);
		case 13: return abs(a);
		case 14: return min(a, b);
		case 15: return max(a, b);
		case 16: return floor(a);
		case 17: return ceil(a);
		case 18: return a - floor(a);
		case 19: return vec3(b.x != 0.0 ? mod(a.x, b.x) : 0.0, b.y != 0.0 ? mod(a.y, b.y) : 0.0, b.z != 0.0 ? mod(a.z, b.z) : 0.0);
		case 20: return floor(lnx_safe_div3(a, b)) * b;
		case 21: return sin(a);
		case 22: return cos(a);
		case 23: return tan(a);
	}
	return vec3(0.0);
}

float lnx_vmath_value(int op, vec3 a, vec3 b) {
	if (op == 8) return dot(a, b);
	if (op == 9) return distance(a, b);
	if (op == 10) return length(a);
	return 0.0;
}

float lnx_map_range(float v, float fmin, float fmax, float tmin, float tmax, float steps, int interp, bool clampResult) {
	float t = fmax != fmin ? (v - fmin) / (fmax - fmin) : 0.0;
	if (interp == 1) t = steps > 0.0 ? floor(t * (steps + 1.0)) / steps : 0.0;
	else if (interp == 2) { t = clamp(t, 0.0, 1.0); t = t * t * (3.0 - 2.0 * t); }
	else if (interp == 3) { t = clamp(t, 0.0, 1.0); t = t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
	float r = tmin + t * (tmax - tmin);
	if (clampResult && interp < 2) r = clamp(r, min(tmin, tmax), max(tmin, tmax));
	return r;
}

vec3 lnx_blend3(int mode, float t, vec3 a, vec3 b) {
	float tm = 1.0 - t;
	switch (mode) {
		case 0: return mix(a, b, t);
		case 1: return mix(a, min(a, b), t);
		case 2: return mix(a, a * b, t);
		case 3: {
			vec3 tmp = tm + t * b;
			return vec3(tmp.x <= 0.0 ? 0.0 : clamp(1.0 - (1.0 - a.x) / tmp.x, 0.0, 1.0),
			            tmp.y <= 0.0 ? 0.0 : clamp(1.0 - (1.0 - a.y) / tmp.y, 0.0, 1.0),
			            tmp.z <= 0.0 ? 0.0 : clamp(1.0 - (1.0 - a.z) / tmp.z, 0.0, 1.0));
		}
		case 4: return mix(a, max(a, b), t);
		case 5: return 1.0 - (tm + t * (1.0 - b)) * (1.0 - a);
		case 6: {
			vec3 tmp = 1.0 - t * b;
			return vec3(a.x == 0.0 ? 0.0 : (tmp.x <= 0.0 ? 1.0 : min(a.x / tmp.x, 1.0)),
			            a.y == 0.0 ? 0.0 : (tmp.y <= 0.0 ? 1.0 : min(a.y / tmp.y, 1.0)),
			            a.z == 0.0 ? 0.0 : (tmp.z <= 0.0 ? 1.0 : min(a.z / tmp.z, 1.0)));
		}
		case 7: return a + t * b;
		case 8: {
			vec3 r;
			for (int i = 0; i < 3; i++)
				r[i] = a[i] < 0.5 ? a[i] * (tm + 2.0 * t * b[i]) : 1.0 - (tm + 2.0 * t * (1.0 - b[i])) * (1.0 - a[i]);
			return r;
		}
		case 9: { vec3 scr = 1.0 - (1.0 - b) * (1.0 - a); return tm * a + t * ((1.0 - a) * b * a + a * scr); }
		case 10: return a + t * (2.0 * b - 1.0);
		case 11: return mix(a, abs(a - b), t);
		case 12: return max(mix(a, a + b - 2.0 * a * b, t), vec3(0.0));
		case 13: return a - t * b;
		case 14: return vec3(b.x != 0.0 ? tm * a.x + t * a.x / b.x : a.x, b.y != 0.0 ? tm * a.y + t * a.y / b.y : a.y, b.z != 0.0 ? tm * a.z + t * a.z / b.z : a.z);
		case 15: { vec3 hb = lnx_rgb2hsv(b); if (hb.y == 0.0) return a; vec3 ha = lnx_rgb2hsv(a); ha.x = hb.x; return mix(a, lnx_hsv2rgb(ha), t); }
		case 16: { vec3 ha = lnx_rgb2hsv(a); if (ha.y == 0.0) return a; ha.y = mix(ha.y, lnx_rgb2hsv(b).y, t); return lnx_hsv2rgb(ha); }
		case 17: { vec3 hb = lnx_rgb2hsv(b); hb.z = lnx_rgb2hsv(a).z; return mix(a, lnx_hsv2rgb(hb), t); }
		case 18: { vec3 ha = lnx_rgb2hsv(a); ha.z = mix(ha.z, lnx_rgb2hsv(b).z, t); return lnx_hsv2rgb(ha); }
	}
	return a;
}

vec4 lnx_blend(int mode, float t, vec4 a, vec4 b, bool clampResult) {
	vec3 r = lnx_blend3(mode, clamp(t, 0.0, 1.0), a.rgb, b.rgb);
	if (clampResult) r = clamp(r, 0.0, 1.0);
	return vec4(r, a.a);
}
// ---------------- end of library ----------------
)GLSL";


	static const char* kLibImage = R"GLSL(
// ---------------- Image texture (Blender: interpolation, projection, extension, color space, alpha) ----------------
vec3 lnx_srgb_to_linear(vec3 c) {
	c = max(c, vec3(0.0));
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

// Bicubic B-spline filtering with 4 bilinear taps
vec4 lnx_texture_bicubic(sampler2D s, vec2 uv) {
	vec2 size = vec2(textureSize(s, 0));
	vec2 st = uv * size - 0.5;
	vec2 i = floor(st), f = st - i;
	vec2 f2 = f * f, f3 = f2 * f;
	vec2 w0 = (1.0 / 6.0) * (-f3 + 3.0 * f2 - 3.0 * f + 1.0);
	vec2 w1 = (1.0 / 6.0) * (3.0 * f3 - 6.0 * f2 + 4.0);
	vec2 w2 = (1.0 / 6.0) * (-3.0 * f3 + 3.0 * f2 + 3.0 * f + 1.0);
	vec2 w3 = (1.0 / 6.0) * f3;
	vec2 g0 = w0 + w1, g1 = w2 + w3;
	vec2 h0 = (w1 / g0) - 0.5 + i, h1 = (w3 / g1) + 1.5 + i;
	vec4 t00 = texture(s, (vec2(h0.x, h0.y) + 0.5) / size), t10 = texture(s, (vec2(h1.x, h0.y) + 0.5) / size);
	vec4 t01 = texture(s, (vec2(h0.x, h1.y) + 0.5) / size), t11 = texture(s, (vec2(h1.x, h1.y) + 0.5) / size);
	return g0.y * (g0.x * t00 + g1.x * t10) + g1.y * (g0.x * t01 + g1.x * t11);
}

// ext: 0 Repeat, 1 Extend, 2 Clip, 3 Mirror   interp: 0 Linear, 1 Closest, 2 Cubic
vec4 lnx_image_sample(sampler2D s, vec2 uv, int ext, int interp) {
	vec2 raw = uv;
	if (ext == 1) uv = clamp(uv, 0.0, 1.0);
	else if (ext == 3) uv = 1.0 - abs(fract(uv * 0.5) * 2.0 - 1.0);
	vec4 c;
	if (interp == 1) {
		vec2 sz = vec2(textureSize(s, 0));
		vec2 p = (ext == 0) ? fract(uv) : clamp(uv, 0.0, 0.99999);
		c = texelFetch(s, ivec2(p * sz), 0);
	}
	else if (interp == 2) c = lnx_texture_bicubic(s, uv);
	else c = texture(s, uv);
	if (ext == 2 && (any(lessThan(raw, vec2(0.0))) || any(greaterThan(raw, vec2(1.0))))) c = vec4(0.0);
	return c;
}

// proj: 0 Flat, 1 Box (triplanar), 2 Sphere, 3 Tube
vec4 lnx_image(sampler2D s, vec3 co, int proj, float blend, int ext, int interp) {
	if (proj == 1) {
		vec3 n = abs(normalize(LNX_NORMAL));
		vec3 w = pow(n, vec3(mix(32.0, 1.0, clamp(blend, 0.0, 1.0))));
		w /= max(w.x + w.y + w.z, 1e-5);
		return lnx_image_sample(s, co.yz, ext, interp) * w.x + lnx_image_sample(s, co.xz, ext, interp) * w.y + lnx_image_sample(s, co.xy, ext, interp) * w.z;
	}
	vec2 uv = co.xy;
	if (proj == 2) {
		vec3 d = co - vec3(0.5);
		uv = vec2(atan(d.y, d.x) / 6.28318530718 + 0.5, atan(d.z, length(d.xy)) / 3.14159265359 + 0.5);
	}
	else if (proj == 3) {
		vec3 d = co - vec3(0.5);
		uv = vec2(atan(d.y, d.x) / 6.28318530718 + 0.5, co.z);
	}
	return lnx_image_sample(s, uv, ext, interp);
}

// colorspace: 0 sRGB, 1 Linear, 2 Non-Color   alpha: 0 Straight, 1 Premultiplied, 2 Channel Packed, 3 None
vec4 lnx_image_post(vec4 c, int colorspace, int alpha) {
	if (alpha == 1 && c.a > 1e-5) c.rgb /= c.a;
	if (alpha == 3) c.a = 1.0;
	if (colorspace == 0) c.rgb = lnx_srgb_to_linear(c.rgb);
	return c;
}
)GLSL";


	static const char* kLibNodes = R"GLSL(
// ---------------- Blender 4.x node library (textures, converters, vectors) ----------------
float lnx_hash_u(uvec4 q) {
	uint h = q.x * 0x9E3779B1u ^ q.y * 0x85EBCA77u ^ q.z * 0xC2B2AE3Du ^ q.w * 0x27D4EB2Fu;
	h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
	return float(h) / 4294967295.0;
}
float lnx_hash14(vec4 p) { return lnx_hash_u(floatBitsToUint(p)); }

// Signed gradient noise in 1-4 dimensions (4D: the W axis shifts the 3D domain)
float lnx_snoise(vec3 p, float w, int dims) {
	if (dims == 1) return lnx_gnoise(vec3(w, 0.37, 0.71));
	if (dims == 2) return lnx_gnoise(vec3(p.xy, 0.0));
	if (dims == 4) return lnx_gnoise(p + vec3(w * 0.731, w * 1.379, w * 0.537));
	return lnx_gnoise(p);
}

// Blender fractal noise: 0 Multifractal, 1 Ridged, 2 Hybrid, 3 fBM, 4 Hetero Terrain
float lnx_fractal(vec3 p, float w, int dims, float detail, float roughness, float lacunarity, float offset, float gain, int type, bool normalizeFbm) {
	detail = clamp(detail, 0.0, 15.0);
	roughness = max(roughness, 0.0);
	int n = int(detail);
	float rmd = detail - floor(detail);
	if (type == 3) {
		float fscale = 1.0, amp = 1.0, maxamp = 0.0, sum = 0.0;
		for (int i = 0; i <= 15; i++) {
			if (i > n) break;
			sum += lnx_snoise(p * fscale, w * fscale, dims) * amp;
			maxamp += amp; amp *= roughness; fscale *= lacunarity;
		}
		if (rmd != 0.0) {
			float sum2 = sum + lnx_snoise(p * fscale, w * fscale, dims) * amp;
			return normalizeFbm ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) : mix(sum, sum2, rmd);
		}
		return normalizeFbm ? 0.5 * sum / maxamp + 0.5 : sum;
	}
	if (type == 0) {
		float value = 1.0, pwr = 1.0;
		for (int i = 0; i <= 15; i++) {
			if (i > n) break;
			value *= (pwr * lnx_snoise(p, w, dims) + 1.0); pwr *= roughness; p *= lacunarity; w *= lacunarity;
		}
		if (rmd != 0.0) value *= (rmd * pwr * lnx_snoise(p, w, dims) + 1.0);
		return value;
	}
	if (type == 4) {
		float pwr = roughness;
		float value = offset + lnx_snoise(p, w, dims);
		p *= lacunarity; w *= lacunarity;
		for (int i = 1; i <= 15; i++) {
			if (i > n) break;
			float inc = (lnx_snoise(p, w, dims) + offset) * pwr * value;
			value += inc; pwr *= roughness; p *= lacunarity; w *= lacunarity;
		}
		if (rmd != 0.0) value += rmd * (lnx_snoise(p, w, dims) + offset) * pwr * value;
		return value;
	}
	if (type == 2) {
		float pwr = 1.0, value = 0.0, weight = 1.0;
		for (int i = 0; i <= 15; i++) {
			if (i > n || weight <= 0.001) break;
			weight = min(weight, 1.0);
			float signal = (lnx_snoise(p, w, dims) + offset) * pwr;
			pwr *= roughness; value += weight * signal; weight *= gain * signal; p *= lacunarity; w *= lacunarity;
		}
		if (rmd != 0.0 && weight > 0.001) { weight = min(weight, 1.0); value += rmd * weight * (lnx_snoise(p, w, dims) + offset) * pwr; }
		return value;
	}
	// Ridged multifractal
	float pwr = roughness;
	float signal = offset - abs(lnx_snoise(p, w, dims));
	signal *= signal;
	float value = signal, weight = 1.0;
	for (int i = 1; i <= 15; i++) {
		if (i > n) break;
		p *= lacunarity; w *= lacunarity;
		weight = clamp(signal * gain, 0.0, 1.0);
		signal = offset - abs(lnx_snoise(p, w, dims));
		signal *= signal; signal *= weight;
		value += signal * pwr; pwr *= roughness;
	}
	return value;
}

vec4 lnx_noise4(vec3 p, float w, int dims, float scale, float detail, float roughness, float lacunarity, float offset, float gain, float distortion, int type, bool normalizeFbm) {
	p *= scale; w *= scale;
	if (distortion != 0.0) {
		p += vec3(lnx_snoise(p + vec3(13.5, 2.1, 7.7), w, dims), lnx_snoise(p + vec3(-7.1, 3.3, 1.9), w, dims), lnx_snoise(p + vec3(3.3, -9.2, 5.4), w, dims)) * distortion;
		w += lnx_snoise(p + vec3(1.7, 9.2, 3.1), w, dims) * distortion;
	}
	float f = lnx_fractal(p, w, dims, detail, roughness, lacunarity, offset, gain, type, normalizeFbm);
	vec3 c = vec3(f,
		lnx_fractal(p + vec3(5.2, 1.3, 7.7), w + 3.1, dims, detail, roughness, lacunarity, offset, gain, type, normalizeFbm),
		lnx_fractal(p + vec3(1.7, 9.2, 3.1), w + 6.7, dims, detail, roughness, lacunarity, offset, gain, type, normalizeFbm));
	return vec4(c, f);
}

// ---- Voronoi (F1, F2, Smooth F1, Distance to Edge, N-Sphere Radius; Euclidean/Manhattan/Chebychev/Minkowski) ----
float lnx_vdist(vec3 a, int metric, float e) {
	a = abs(a);
	if (metric == 1) return a.x + a.y + a.z;
	if (metric == 2) return max(a.x, max(a.y, a.z));
	if (metric == 3) return pow(pow(a.x, e) + pow(a.y, e) + pow(a.z, e), 1.0 / max(e, 1e-4));
	return length(a);
}

void lnx_voronoi_cell(vec3 p, float randomness, int feature, int metric, float exponent, float smoothness, bool isFlat,
                      out float dist, out vec3 col, out vec3 pos, out float radius) {
	vec3 cell = floor(p);
	vec3 local = p - cell;
	randomness = clamp(randomness, 0.0, 1.0);
	float d1 = 8.0, d2 = 8.0;
	vec3 p1 = vec3(0.0), c1 = cell, p2 = vec3(0.0);
	int zr = isFlat ? 0 : 1;
	if (feature == 2) {
		// Smooth F1 (Blender)
		float s = clamp(smoothness * 0.5, 0.0, 0.5);
		float sd = 8.0; vec3 sc = vec3(0.0), sp = vec3(0.0); float h = -1.0;
		int zr2 = isFlat ? 0 : 2;
		for (int z = -zr2; z <= zr2; z++) for (int y = -2; y <= 2; y++) for (int x = -2; x <= 2; x++) {
			vec3 off = vec3(float(x), float(y), float(z));
			vec3 pt = off + lnx_hash33(cell + off) * randomness;
			if (isFlat) pt.z = 0.0;
			float d = lnx_vdist(pt - local, metric, exponent);
			h = h == -1.0 ? 1.0 : smoothstep(0.0, 1.0, 0.5 + 0.5 * (sd - d) / max(s, 1e-5));
			float corr = s * h * (1.0 - h);
			sd = mix(sd, d, h) - corr;
			corr /= 1.0 + 3.0 * s;
			sc = mix(sc, lnx_hash33(cell + off + vec3(7.31, 1.17, 3.53)), h) - corr;
			sp = mix(sp, pt, h) - corr;
		}
		dist = sd; col = sc; pos = cell + sp; radius = 0.0;
		return;
	}
	if (feature == 3) {
		// Distance to edge
		vec3 toClosest = vec3(0.0); float minD = 8.0;
		for (int z = -zr; z <= zr; z++) for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
			vec3 off = vec3(float(x), float(y), float(z));
			vec3 v = off + lnx_hash33(cell + off) * randomness - local;
			if (isFlat) v.z = 0.0;
			float d = dot(v, v);
			if (d < minD) { minD = d; toClosest = v; }
		}
		minD = 8.0;
		for (int z = -zr; z <= zr; z++) for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
			vec3 off = vec3(float(x), float(y), float(z));
			vec3 v = off + lnx_hash33(cell + off) * randomness - local;
			if (isFlat) v.z = 0.0;
			vec3 perp = v - toClosest;
			if (dot(perp, perp) > 0.0001) minD = min(minD, dot((toClosest + v) * 0.5, normalize(perp)));
		}
		dist = minD; col = vec3(0.0); pos = vec3(0.0); radius = 0.0;
		return;
	}
	for (int z = -zr; z <= zr; z++) for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
		vec3 off = vec3(float(x), float(y), float(z));
		vec3 pt = off + lnx_hash33(cell + off) * randomness;
		if (isFlat) pt.z = 0.0;
		float d = lnx_vdist(pt - local, metric, exponent);
		if (d < d1) { d2 = d1; p2 = p1; d1 = d; p1 = pt; c1 = cell + off; }
		else if (d < d2) { d2 = d; p2 = pt; }
	}
	if (feature == 1) { dist = d2; col = lnx_hash33(cell + p2 + vec3(7.31, 1.17, 3.53)); pos = cell + p2; }
	else { dist = d1; col = lnx_hash33(c1 + vec3(7.31, 1.17, 3.53)); pos = cell + p1; }
	radius = 0.5 * distance(p1, p2);
}

void lnx_voronoi4(vec3 p, float w, int dims, float scale, float detail, float roughness, float lacunarity, float smoothness, float exponent,
                  float randomness, int feature, int metric, bool normalizeOut,
                  out float dist, out vec3 col, out vec3 pos, out float wout, out float radius) {
	if (dims == 1) p = vec3(w, 0.0, 0.0);
	else if (dims == 4) p += vec3(w * 0.731, w * 1.379, w * 0.537);
	bool isFlat = dims <= 2;
	if (isFlat) p.z = 0.0;
	p *= scale;
	float d0; vec3 col0, pos0; float r0;
	lnx_voronoi_cell(p, randomness, feature, metric, exponent, smoothness, isFlat, d0, col0, pos0, r0);
	dist = d0; col = col0; pos = pos0 / max(scale, 1e-5); radius = r0; wout = w;
	detail = clamp(detail, 0.0, 15.0);
	if (detail > 0.0 && feature <= 2) {
		float amp = 1.0, maxAmp = 1.0, fscale = lacunarity;
		float sumD = d0; vec3 sumC = col0;
		for (int i = 1; i <= 15; i++) {
			if (float(i) > detail) break;
			amp *= roughness;
			float d; vec3 c, ps; float r;
			lnx_voronoi_cell(p * fscale, randomness, feature, metric, exponent, smoothness, isFlat, d, c, ps, r);
			sumD += d * amp; sumC += c * amp; maxAmp += amp; fscale *= lacunarity;
		}
		dist = normalizeOut ? sumD / maxAmp : sumD / maxAmp * 1.0;
		col = sumC / maxAmp;
	}
	if (normalizeOut && feature != 3) dist = clamp(dist / (feature == 1 ? 1.5 : 1.0), 0.0, 1.0);
}

)GLSL";

	static const char* kLibNodes2 = R"GLSL(
// ---- Wave (Blender 4.x) ----
float lnx_wave4(vec3 p, float scale, float distortion, float detail, float detailScale, float detailRough, float phase, int type, int dir, int profile) {
	p = (p * scale + 0.000001) * 0.999999;
	float n;
	if (type == 0) {
		if (dir == 0) n = p.x * 20.0; else if (dir == 1) n = p.y * 20.0; else if (dir == 2) n = p.z * 20.0; else n = (p.x + p.y + p.z) * 10.0;
	}
	else {
		vec3 rp = p;
		if (dir == 0) rp *= vec3(0.0, 1.0, 1.0); else if (dir == 1) rp *= vec3(1.0, 0.0, 1.0); else if (dir == 2) rp *= vec3(1.0, 1.0, 0.0);
		n = length(rp) * 20.0;
	}
	n += phase;
	if (distortion != 0.0) n += distortion * (lnx_fbm(p * detailScale, detail, detailRough) * 2.0 - 1.0);
	if (profile == 0) return 0.5 + 0.5 * sin(n - 1.5707963);
	n /= 6.2831853;
	if (profile == 1) return n - floor(n);
	return abs(n - floor(n + 0.5)) * 2.0;
}

float lnx_gradient4(vec3 p, int type) {
	float r;
	if (type == 0) r = p.x;
	else if (type == 1) { r = max(p.x, 0.0); r *= r; }
	else if (type == 2) { r = clamp(p.x, 0.0, 1.0); float t = r * r; r = 3.0 * t - 2.0 * t * r; }
	else if (type == 3) r = (p.x + p.y) * 0.5;
	else if (type == 4) r = max(0.999999 - length(p), 0.0);
	else if (type == 5) { r = max(0.999999 - length(p), 0.0); r *= r; }
	else r = atan(p.y, p.x) / 6.2831853 + 0.5;
	return clamp(r, 0.0, 1.0);
}

// ---- Brick (Blender) ----
float lnx_brick_noise(uint n) {
	n = (n + 1013u) & 0x7fffffffu;
	n = (n >> 13u) ^ n;
	uint nn = (n * (n * n * 60493u + 19990303u) + 1376312589u) & 0x7fffffffu;
	return 0.5 * (float(nn) / 1073741824.0);
}
vec2 lnx_brick(vec3 p, float mortarSize, float mortarSmooth, float bias, float brickWidth, float rowHeight,
               float offsetAmount, int offsetFreq, float squashAmount, int squashFreq) {
	int rownum = int(floor(p.y / rowHeight));
	float offset = 0.0;
	if (offsetFreq != 0 && squashFreq != 0) {
		brickWidth *= ((rownum % squashFreq) != 0) ? 1.0 : squashAmount;
		offset = ((rownum % offsetFreq) != 0) ? 0.0 : (brickWidth * offsetAmount);
	}
	int bricknum = int(floor((p.x + offset) / brickWidth));
	float x = (p.x + offset) - brickWidth * float(bricknum);
	float y = p.y - rowHeight * float(rownum);
	float tint = clamp(lnx_brick_noise(uint((rownum << 16) + (bricknum & 0xFFFF))) + bias, 0.0, 1.0);
	float minDist = min(min(x, y), min(brickWidth - x, rowHeight - y));
	if (minDist >= mortarSize) return vec2(tint, 0.0);
	if (mortarSmooth == 0.0) return vec2(tint, 1.0);
	minDist = 1.0 - minDist / mortarSize;
	return vec2(tint, smoothstep(0.0, mortarSmooth, minDist));
}

// ---- Magic (Blender) ----
vec4 lnx_magic(vec3 p, float scale, float distortion, int depth) {
	p = mod(p * scale, 6.28318530718);
	float x = sin((p.x + p.y + p.z) * 5.0);
	float y = cos((-p.x + p.y - p.z) * 5.0);
	float z = -cos((-p.x - p.y + p.z) * 5.0);
	if (depth > 0) { x *= distortion; y *= distortion; z *= distortion; y = -cos(x - y + z); y *= distortion;
	if (depth > 1) { x = cos(x - y - z); x *= distortion;
	if (depth > 2) { z = sin(-x - y - z); z *= distortion;
	if (depth > 3) { x = -cos(-x + y - z); x *= distortion;
	if (depth > 4) { y = -sin(-x + y + z); y *= distortion;
	if (depth > 5) { y = -cos(-x + y + z); y *= distortion;
	if (depth > 6) { x = cos(x + y + z); x *= distortion;
	if (depth > 7) { z = sin(x + y - z); z *= distortion;
	if (depth > 8) { x = -cos(-x - y + z); x *= distortion;
	if (depth > 9) { y = -sin(x - y + z); y *= distortion; } } } } } } } } } }
	if (distortion != 0.0) { distortion *= 2.0; x /= distortion; y /= distortion; z /= distortion; }
	vec3 c = vec3(0.5 - x, 0.5 - y, 0.5 - z);
	return vec4(c, (c.x + c.y + c.z) / 3.0);
}

// ---- Gabor (sparse convolution approximation) ----
vec3 lnx_gabor(vec3 p, float scale, float frequency, float anisotropy, vec3 orientation, bool is3D) {
	p *= scale;
	if (!is3D) p.z = 0.0;
	vec3 cell = floor(p);
	float re = 0.0, im = 0.0;
	int zr = is3D ? 1 : 0;
	for (int z = -zr; z <= zr; z++) for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
		vec3 c = cell + vec3(float(x), float(y), float(z));
		for (int k = 0; k < 2; k++) {
			vec3 h = lnx_hash33(c + vec3(float(k) * 17.3, 3.1, 7.7));
			vec3 center = c + h;
			if (!is3D) center.z = 0.0;
			vec3 d = p - center;
			float g = exp(-3.14159265 * dot(d, d));
			vec3 rnd = normalize(lnx_hash33(c + vec3(5.3, float(k), 1.9)) * 2.0 - 1.0);
			if (!is3D) rnd.z = 0.0;
			vec3 dir = normalize(mix(rnd, normalize(orientation), clamp(anisotropy, 0.0, 1.0)) + vec3(1e-5));
			float ph = 6.28318530718 * frequency * dot(d, dir);
			float weight = h.z * 2.0 - 1.0;
			re += weight * g * cos(ph);
			im += weight * g * sin(ph);
		}
	}
	float intensity = sqrt(re * re + im * im);
	return vec3(clamp(re * 0.5 + 0.5, 0.0, 1.0), atan(im, re) / 6.28318530718 + 0.5, intensity);
}

// ---- Sky (Nishita-like analytic approximation) ----
vec3 lnx_sky(vec3 dir, float sunSize, float sunIntensity, float elevation, float rotation, float air, float dust, float ozone, bool disc) {
	dir = normalize(dir);
	float el = radians(elevation), rot = radians(rotation);
	vec3 sun = normalize(vec3(cos(el) * sin(rot), sin(el), cos(el) * cos(rot)));
	float h = max(dir.y, 0.0);
	vec3 zenith = vec3(0.10, 0.28, 0.75) * (0.6 + 0.4 * air) * mix(vec3(1.0), vec3(0.85, 0.9, 1.0), clamp(ozone * 0.5, 0.0, 1.0));
	vec3 horizon = vec3(0.75, 0.85, 1.0) * (1.0 + dust * 0.08);
	vec3 col = mix(horizon, zenith, pow(h, 0.45));
	float sd = max(dot(dir, sun), 0.0);
	col += vec3(1.0, 0.75, 0.5) * pow(sd, 8.0) * 0.35 * dust;
	col *= clamp(sun.y * 2.5 + 0.35, 0.03, 1.0);
	if (sun.y < 0.2) col = mix(col, col * vec3(1.0, 0.55, 0.35), clamp(1.0 - sun.y * 5.0, 0.0, 1.0) * 0.6);
	if (dir.y < 0.0) col = mix(col, vec3(0.08, 0.07, 0.06), clamp(-dir.y * 6.0, 0.0, 1.0));
	if (disc && dot(dir, sun) > cos(radians(max(sunSize, 0.05)) * 0.5)) col += vec3(1.0, 0.95, 0.85) * 20.0;
	return col * sunIntensity;
}

// Equirectangular / mirror ball lookup (engine is Y-up, Blender is Z-up)
vec2 lnx_env_uv(vec3 d, int projection) {
	d = normalize(d);
	vec3 b = vec3(d.x, -d.z, d.y);
	if (projection == 1) {
		b.y -= 1.0;
		float div = 2.0 * sqrt(max(-0.5 * b.y, 0.0));
		if (div > 0.0) b /= div;
		return vec2(0.5 * (b.x + 1.0), 0.5 * (b.z + 1.0));
	}
	return vec2(-atan(b.y, b.x) / 6.28318530718 + 0.5, atan(b.z, length(b.xy)) / 3.14159265359 + 0.5);
}

// ---- Converters ----
vec3 lnx_blackbody(float t) {
	t = clamp(t, 800.0, 12000.0) / 100.0;
	vec3 c;
	c.r = t <= 66.0 ? 1.0 : clamp(1.29293618606 * pow(t - 60.0, -0.1332047592), 0.0, 1.0);
	c.g = t <= 66.0 ? clamp(0.39008157876 * log(t) - 0.63184144378, 0.0, 1.0) : clamp(1.12989086089 * pow(t - 60.0, -0.0755148492), 0.0, 1.0);
	c.b = t >= 66.0 ? 1.0 : (t <= 19.0 ? 0.0 : clamp(0.54320678911 * log(t - 10.0) - 1.19625408914, 0.0, 1.0));
	return lnx_srgb_to_linear(c);
}

vec3 lnx_wavelength(float w) {
	vec3 c;
	if (w < 440.0) c = vec3(-(w - 440.0) / 60.0, 0.0, 1.0);
	else if (w < 490.0) c = vec3(0.0, (w - 440.0) / 50.0, 1.0);
	else if (w < 510.0) c = vec3(0.0, 1.0, -(w - 510.0) / 20.0);
	else if (w < 580.0) c = vec3((w - 510.0) / 70.0, 1.0, 0.0);
	else if (w < 645.0) c = vec3(1.0, -(w - 645.0) / 65.0, 0.0);
	else c = vec3(1.0, 0.0, 0.0);
	float f = w < 420.0 ? 0.3 + 0.7 * (w - 380.0) / 40.0 : (w > 700.0 ? 0.3 + 0.7 * (780.0 - w) / 80.0 : 1.0);
	return lnx_srgb_to_linear(clamp(c * f, 0.0, 1.0));
}

// Monotone cubic curve segment: a/b = (x, y, tangent, 0)
float lnx_hermite(vec4 a, vec4 b, float x) {
	float h = max(b.x - a.x, 1e-6);
	float t = clamp((x - a.x) / h, 0.0, 1.0);
	float t2 = t * t, t3 = t2 * t;
	return (2.0 * t3 - 3.0 * t2 + 1.0) * a.y + (t3 - 2.0 * t2 + t) * h * a.z + (-2.0 * t3 + 3.0 * t2) * b.y + (t3 - t2) * h * b.z;
}

vec3 lnx_rotate_axis(vec3 v, vec3 axis, float angle) {
	axis = length(axis) > 0.0 ? normalize(axis) : vec3(0.0, 0.0, 1.0);
	float c = cos(angle), s = sin(angle);
	return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
}
)GLSL";

	// ============================================================================
	// CODE GENERATOR
	// ============================================================================

	namespace {
		enum class T { Float, Vec2, Vec3, Vec4, BSDF };

		T TypeOf(PinType p) {
			switch (p) {
				case PinType::Vec2: return T::Vec2;
				case PinType::Vec3: return T::Vec3;
				case PinType::Color:
				case PinType::Texture: return T::Vec4;
				case PinType::Shader: return T::BSDF;
				default: return T::Float;
			}
		}

		const char* GL(T t) {
			switch (t) {
				case T::Vec2: return "vec2";
				case T::Vec3: return "vec3";
				case T::Vec4: return "vec4";
				case T::BSDF: return "LnxBSDF";
				default: return "float";
			}
		}

		std::string Conv(const std::string& e, T from, T to) {
			if (from == to) return e;
			if (to == T::BSDF || from == T::BSDF) return "lnx_bsdf_default()";
			switch (to) {
				case T::Float:
					if (from == T::Vec4) return "lnx_luma((" + e + ").rgb)";
					return "lnx_avg(" + e + ")";
				case T::Vec2:
					if (from == T::Float) return "vec2(" + e + ")";
					return "(" + e + ").xy";
				case T::Vec3:
					if (from == T::Float) return "vec3(" + e + ")";
					if (from == T::Vec2) return "vec3(" + e + ", 0.0)";
					return "(" + e + ").xyz";
				case T::Vec4:
					if (from == T::Float) return "vec4(vec3(" + e + "), 1.0)";
					if (from == T::Vec2) return "vec4(" + e + ", 0.0, 1.0)";
					return "vec4(" + e + ", 1.0)";
				default: return e;
			}
		}

		std::string Lit(float v) {
			char buf[32];
			snprintf(buf, sizeof(buf), "%.6f", v);
			return buf;
		}

		class Generator {
		public:
			using Bindings = std::unordered_map<std::string, std::pair<std::string, T>>;

			Generator(const NodeGraph& graph, CompiledGraph& out) : m_Graph(graph), m_Out(out), m_Body(m_OwnBody) {}
			// Group subgraph: appends to the parent's body; Group Input sockets read `inputs`
			Generator(const NodeGraph& graph, CompiledGraph& out, std::string& body, const Bindings* inputs)
				: m_Graph(graph), m_Out(out), m_Body(body), m_GroupInputs(inputs) {}

			std::string Surface() {
				if (const Node* out = m_Graph.FindNodeByType(MaterialOutput)) {
					const std::string surface = In(*out, "Surface", T::BSDF);
					const Pin* disp = out->FindInput("Displacement");
					if (!disp || !m_Graph.GetLinkInto(disp->PinID)) return surface;
					Line("LnxBSDF lnx_surface = " + surface + ";");
					Line("lnx_surface.Normal = lnx_bump(1.0, 1.0, dot(" + In(*out, "Displacement", T::Vec3) + ", LNX_NORMAL), normalize(lnx_surface.Normal), false);");
					return "lnx_surface";
				}
				if (const Node* legacy = m_Graph.FindNodeByType(OutputNode))
					return LegacyOutput(*legacy);
				m_Out.Errors.push_back("The graph has no Material Output node");
				return "lnx_bsdf_default()";
			}

			const std::string& Body() const { return m_Body; }

			// Node previews: vec4 of the node's first output (closures show base color + emission)
			std::string PreviewExpr(const Node& n) {
				Emit(n);
				for (const auto& o : n.Outputs) {
					if (o.Name == NewSocketPin) continue;
					auto it = m_OutVars.find(o.PinID);
					if (it == m_OutVars.end()) continue;
					const T type = m_OutTypes[o.PinID];
					if (type == T::BSDF) return "vec4(" + it->second + ".BaseColor + " + it->second + ".Emission, 1.0)";
					if (type == T::Vec3 && o.Type == PinType::Vec3) return "vec4(" + it->second + ", 1.0)";
					return Conv(it->second, type, T::Vec4);
				}
				return "vec4(0.0, 0.0, 0.0, 1.0)";
			}

		private:
			// ---- Params / textures ----
			int AddParam(const ParamRef& ref) {
				m_Out.Params.push_back(ref);
				const int slot = static_cast<int>(m_Out.Params.size());   // slot 0 is time
				if (slot >= kMaxGraphParams && m_Out.Errors.empty())
					m_Out.Errors.push_back("Too many constant values in the graph (max " + std::to_string(kMaxGraphParams - 1) + ")");
				return slot;
			}

			static std::string ParamExpr(int slot, T t) {
				const std::string base = "u_GraphParams[" + std::to_string(std::min(slot, kMaxGraphParams - 1)) + "]";
				switch (t) {
					case T::Float: return base + ".x";
					case T::Vec2: return base + ".xy";
					case T::Vec3: return base + ".xyz";
					default: return base;
				}
			}

			int AddTexture(const std::string& path) {
				for (size_t i = 0; i < m_Out.TexturePaths.size(); i++)
					if (m_Out.TexturePaths[i] == path) return static_cast<int>(i);
				if (static_cast<int>(m_Out.TexturePaths.size()) >= kMaxGraphTextures) {
					m_Out.Errors.push_back("Too many image textures (max " + std::to_string(kMaxGraphTextures) + ")");
					return -1;
				}
				m_Out.TexturePaths.push_back(path);
				return static_cast<int>(m_Out.TexturePaths.size()) - 1;
			}

			// ---- Helpers ----
			static std::string Prop(const Node& n, const char* key, const char* fallback = "") {
				auto it = n.Properties.find(key);
				return it != n.Properties.end() ? it->second : fallback;
			}

			int Op(const Node& n, const char* key) {
				const NodeTypeInfo* info = m_Graph.GetTypeInfo(n.TypeName);
				if (!info) return 0;
				const std::string value = Prop(n, key);
				for (const auto& e : info->Enums) {
					if (e.Key != key) continue;
					for (size_t i = 0; i < e.Options.size(); i++) if (e.Options[i] == value) return static_cast<int>(i);
				}
				return 0;
			}

			bool Toggle(const Node& n, const char* key) { return Prop(n, key, "0") == "1"; }

			// A socket that matters at runtime: linked, or its value past `threshold`
			bool PinActive(const Node& n, const char* name, float threshold, bool greater) {
				const Pin* p = n.FindInput(name);
				if (!p) return false;
				if (m_Graph.GetLinkInto(p->PinID)) return true;
				return greater ? p->DefaultValue.x > threshold : p->DefaultValue.x < threshold;
			}

			// Curve lookup (RGB / Vector / Float Curves): points live in the params UBO as (x, y, tangent)
			std::string Curve(const Node& n, const std::string& key, const std::string& x) {
				auto it = n.Properties.find(key);
				const auto points = ParseCurve(it != n.Properties.end() ? it->second : "");
				std::vector<std::string> P;
				for (int i = 0; i < static_cast<int>(points.size()); i++) {
					ParamRef ref{ n.NodeID, 0, i, false };
					ref.Curve = key;
					P.push_back(ParamExpr(AddParam(ref), T::Vec4));
				}
				const std::string v = "n" + std::to_string(n.NodeID) + "_cv" + std::to_string(m_CurveCounter++);
				Line("float " + v + " = " + P[0] + ".y;");
				Line("{ float cx = " + x + ";");
				for (size_t i = 0; i + 1 < P.size(); i++) Line("if (cx > " + P[i] + ".x) " + v + " = lnx_hermite(" + P[i] + ", " + P[i + 1] + ", cx);");
				Line("if (cx >= " + P.back() + ".x) " + v + " = " + P.back() + ".y; }");
				return v;
			}

			std::string Var(const Node& n, const std::string& outName) {
				for (size_t i = 0; i < n.Outputs.size(); i++)
					if (n.Outputs[i].Name == outName) return "n" + std::to_string(n.NodeID) + "_" + std::to_string(i);
				return "n" + std::to_string(n.NodeID) + "_x";
			}

			void Decl(const Node& n, const std::string& outName, T type, const std::string& expr) {
				const Pin* pin = n.FindOutput(outName);
				const std::string var = Var(n, outName);
				m_Body += "\t" + std::string(GL(type)) + " " + var + " = " + expr + ";\n";
				if (pin) { m_OutVars[pin->PinID] = var; m_OutTypes[pin->PinID] = type; }
			}

			void Line(const std::string& code) { m_Body += "\t" + code + "\n"; }

			// Expression for an input socket, converted to `want`
			std::string In(const Node& n, const char* name, T want, const char* implicitExpr = nullptr, T implicitType = T::Vec3) {
				const Pin* pin = n.FindInput(name);
				if (!pin) return Conv("0.0", T::Float, want);

				if (m_Graph.GetLinkInto(pin->PinID)) {
					const auto [src, srcPin] = m_Graph.GetSource(pin->PinID);   // reroutes walked through
					if (src && srcPin) {
						Emit(*src);
						auto it = m_OutVars.find(srcPin->PinID);
						if (it != m_OutVars.end()) return Conv(it->second, m_OutTypes[srcPin->PinID], want);
					}
				}

				if (want == T::BSDF || pin->Type == PinType::Shader) return "lnx_bsdf_default()";
				if (implicitExpr) return Conv(implicitExpr, implicitType, want);
				if (pin->HideValue) {
					if (pin->Type == PinType::Vec3) return Conv("LNX_NORMAL", T::Vec3, want);
					return Conv("0.0", T::Float, want);
				}
				const int slot = AddParam({ n.NodeID, pin->PinID });
				return Conv(ParamExpr(slot, TypeOf(pin->Type)), TypeOf(pin->Type), want);
			}

			bool Visible(const Node& n, const char* pinName) {
				const NodeTypeInfo* info = m_Graph.GetTypeInfo(n.TypeName);
				const Pin* p = n.FindInput(pinName);
				return !(info && info->PinVisible && p) || info->PinVisible(n, *p);
			}

			// ---- Nodes ----
			void Emit(const Node& n) {
				if (m_Done.count(n.NodeID)) return;
				if (m_Visiting.count(n.NodeID)) {
					m_Out.Errors.push_back("The graph contains a cycle (" + n.Title + ")");
					return;
				}
				m_Visiting.insert(n.NodeID);
				EmitNode(n);
				m_Visiting.erase(n.NodeID);
				m_Done.insert(n.NodeID);
			}

			void EmitNode(const Node& n) {
				const std::string& t = n.TypeName;
				const std::string id = "n" + std::to_string(n.NodeID);

				// ---------------- Groups ----------------
				if (t == GroupType) {
					EmitGroup(n);
					return;
				}
				if (t == GroupInputType) {
					for (const auto& o : n.Outputs) {
						if (o.Name == NewSocketPin) continue;
						const T type = TypeOf(o.Type);
						std::string expr = Conv("0.0", T::Float, type);
						if (m_GroupInputs) {
							auto it = m_GroupInputs->find(o.Name);
							if (it != m_GroupInputs->end()) expr = Conv(it->second.first, it->second.second, type);
						}
						Decl(n, o.Name, type, expr);
					}
					return;
				}
				if (t == RerouteType || t == GroupOutputType) {
					for (const auto& o : n.Outputs) Decl(n, o.Name, TypeOf(o.Type), Conv("0.0", T::Float, TypeOf(o.Type)));
					return;
				}

				// ---------------- Shaders ----------------
				if (t == PrincipledBSDF) {
					const std::string b = Var(n, "BSDF");
					Line("LnxBSDF " + b + " = lnx_bsdf_default();");
					Line(b + ".BaseColor = max(" + In(n, "Base Color", T::Vec3) + ", vec3(0.0));");
					Line(b + ".Metallic = clamp(" + In(n, "Metallic", T::Float) + ", 0.0, 1.0);");
					Line(b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					Line(b + ".IOR = max(" + In(n, "IOR", T::Float) + ", 1.0);");
					// Blender 4: F0 from IOR, scaled by "IOR Level" (0.5 = physical)
					Line("{ float f0 = (" + b + ".IOR - 1.0) / (" + b + ".IOR + 1.0); f0 *= f0; " + b + ".Specular = clamp(f0 / 0.08 * " + In(n, "Specular", T::Float) + " * 2.0, 0.0, 12.5); }");
					Line(b + ".SpecularTint = max(" + In(n, "Specular Tint", T::Vec3) + ", vec3(0.0));");
					Line(b + ".Alpha = clamp(" + In(n, "Alpha", T::Float) + ", 0.0, 1.0);");
					Line(b + ".Normal = normalize(" + In(n, "Normal", T::Vec3, "LNX_NORMAL") + ");");
					Line(b + ".AO = clamp(" + In(n, "Ambient Occlusion", T::Float) + ", 0.0, 1.0);");
					// Subsurface color: the explicit "Subsurface Color" when linked / edited, else the Base Color
					// tinted by the per-channel radius (Blender 4 has no separate SSS color)
					const Pin* sc = n.FindInput("Subsurface Color");
					const bool customSss = sc && (m_Graph.GetLinkInto(sc->PinID) || glm::length(sc->DefaultValue - glm::vec4(1.0f, 0.35f, 0.25f, 1.0f)) > 1e-3f);
					std::string sssColor;
					if (customSss) sssColor = In(n, "Subsurface Color", T::Vec3);
					else {
						const std::string r = "n" + std::to_string(n.NodeID) + "_rad";
						Line("vec3 " + r + " = max(" + In(n, "Subsurface Radius", T::Vec3) + ", vec3(0.0));");
						sssColor = b + ".BaseColor * (" + r + " / max(max(" + r + ".x, " + r + ".y), max(" + r + ".z, 1e-4)))";
					}
					Line(b + ".Subsurface = vec4(" + sssColor + ", clamp(" + In(n, "Subsurface Weight", T::Float) + ", 0.0, 1.0));");
					Line(b + ".Transmission = clamp(" + In(n, "Transmission Weight", T::Float) + ", 0.0, 1.0);");
					Line(b + ".Coat = clamp(" + In(n, "Coat Weight", T::Float) + ", 0.0, 1.0);");
					Line(b + ".CoatRoughness = clamp(" + In(n, "Coat Roughness", T::Float) + ", 0.0, 1.0);");
					Line(b + ".CoatIOR = max(" + In(n, "Coat IOR", T::Float) + ", 1.0);");
					Line(b + ".CoatTint = max(" + In(n, "Coat Tint", T::Vec3) + ", vec3(0.0));");
					Line(b + ".Sheen = clamp(" + In(n, "Sheen Weight", T::Float) + ", 0.0, 1.0);");
					Line(b + ".SheenRoughness = clamp(" + In(n, "Sheen Roughness", T::Float) + ", 0.0, 1.0);");
					Line(b + ".SheenTint = max(" + In(n, "Sheen Tint", T::Vec3) + ", vec3(0.0));");
					Line(b + ".Emission = " + In(n, "Emission Color", T::Vec3) + " * " + In(n, "Emission Strength", T::Float) + ";");
					m_Transmission |= PinActive(n, "Transmission Weight", 0.001f, true);
					m_PartialAlpha |= PinActive(n, "Alpha", 0.999f, false);
					RegisterOut(n, "BSDF", T::BSDF);
					return;
				}
				if (t == EmissionShader) {
					const std::string b = Var(n, "Emission");
					Line("LnxBSDF " + b + " = lnx_bsdf_default();");
					Line(b + ".BaseColor = vec3(0.0); " + b + ".Specular = 0.0; " + b + ".Roughness = 1.0;");
					Line(b + ".Emission = " + In(n, "Color", T::Vec3) + " * " + In(n, "Strength", T::Float) + ";");
					RegisterOut(n, "Emission", T::BSDF);
					return;
				}
				if (t == MixShader) {
					Decl(n, "Shader", T::BSDF, "lnx_bsdf_mix(" + In(n, "Shader A", T::BSDF) + ", " + In(n, "Shader B", T::BSDF) + ", " + In(n, "Fac", T::Float) + ")");
					return;
				}
				if (t == AddShader) {
					Decl(n, "Shader", T::BSDF, "lnx_bsdf_add(" + In(n, "Shader A", T::BSDF) + ", " + In(n, "Shader B", T::BSDF) + ")");
					return;
				}
				// Simple closures: start from the default BSDF and set what they define
				auto closure = [&](const char* outName) {
					const std::string b = Var(n, outName);
					Line("LnxBSDF " + b + " = lnx_bsdf_default();");
					if (const Pin* np = n.FindInput("Normal")) { (void)np; Line(b + ".Normal = normalize(" + In(n, "Normal", T::Vec3, "LNX_NORMAL") + ");"); }
					RegisterOut(n, outName, T::BSDF);
					return b;
				};
				if (t == DiffuseBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + "; " + b + ".Specular = 0.0; " + b + ".Roughness = 1.0;");
					return;
				}
				if (t == GlossyBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + "; " + b + ".Metallic = 1.0; " + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					return;
				}
				if (t == MetallicBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Base Color", T::Vec3) + "; " + b + ".Metallic = 1.0; " + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					Line(b + ".SpecularTint = " + In(n, "Edge Tint", T::Vec3) + ";");
					return;
				}
				if (t == GlassBSDF || t == RefractionBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + "; " + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					Line(b + ".IOR = max(" + In(n, "IOR", T::Float) + ", 1.0); " + b + ".Transmission = 1.0; " + b + ".Refract = 1.0;");
					if (t == GlassBSDF) Line("{ float f0 = (" + b + ".IOR - 1.0) / (" + b + ".IOR + 1.0); " + b + ".Specular = f0 * f0 / 0.08; }");
					else Line(b + ".Specular = 0.0;");
					m_Transmission = true;
					return;
				}
				if (t == TransparentBSDF || t == RayPortalBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + "; " + b + ".Specular = 0.0; " + b + ".Roughness = 0.0;");
					Line(b + ".Transmission = 1.0; " + b + ".Refract = 0.0; " + b + ".IOR = 1.0;");
					m_Transmission = true;
					return;
				}
				if (t == TranslucentBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + "; " + b + ".Specular = 0.0; " + b + ".Roughness = 1.0;");
					Line(b + ".Subsurface = vec4(vec3(1.0), 1.0);");
					return;
				}
				if (t == SubsurfaceBSDF) {
					const std::string b = closure("BSSRDF");
					const std::string r = "n" + std::to_string(n.NodeID) + "_rad";
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + "; " + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					Line("vec3 " + r + " = max(" + In(n, "Radius", T::Vec3) + ", vec3(0.0));");
					Line(b + ".Subsurface = vec4(" + b + ".BaseColor * (" + r + " / max(max(" + r + ".x, " + r + ".y), max(" + r + ".z, 1e-4))), 1.0);");
					return;
				}
				if (t == SheenBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = vec3(0.0); " + b + ".Specular = 0.0; " + b + ".Sheen = 1.0;");
					Line(b + ".SheenTint = " + In(n, "Color", T::Vec3) + "; " + b + ".SheenRoughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					return;
				}
				if (t == ToonBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + ";");
					if (Op(n, "Component") == 1) Line(b + ".Metallic = 1.0; " + b + ".Roughness = clamp(" + In(n, "Size", T::Float) + " * 0.5, 0.0, 1.0);");
					else Line(b + ".Specular = 0.0; " + b + ".Roughness = 1.0;");
					return;
				}
				if (t == HairBSDF) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + "; " + b + ".Roughness = clamp(" + In(n, "Roughness U", T::Float) + ", 0.0, 1.0);");
					return;
				}
				if (t == PrincipledHair) {
					const std::string b = closure("BSDF");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + " * " + In(n, "Tint", T::Vec3) + "; " + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					Line(b + ".Coat = clamp(" + In(n, "Coat", T::Float) + ", 0.0, 1.0);");
					return;
				}
				if (t == SpecularBSDF) {
					const std::string b = closure("BSDF");
					const std::string s = "n" + std::to_string(n.NodeID) + "_spec";
					Line(b + ".BaseColor = " + In(n, "Base Color", T::Vec3) + "; " + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);");
					Line("vec3 " + s + " = max(" + In(n, "Specular", T::Vec3) + ", vec3(0.0));");
					Line(b + ".Specular = lnx_luma(" + s + ") / 0.08; " + b + ".SpecularTint = " + s + " / max(lnx_luma(" + s + "), 1e-4);");
					Line(b + ".Emission = " + In(n, "Emissive Color", T::Vec3) + ";");
					Line(b + ".Alpha = 1.0 - clamp(" + In(n, "Transparency", T::Float) + ", 0.0, 1.0);");
					Line(b + ".Coat = clamp(" + In(n, "Clear Coat", T::Float) + ", 0.0, 1.0); " + b + ".CoatRoughness = clamp(" + In(n, "Clear Coat Roughness", T::Float) + ", 0.0, 1.0);");
					Line(b + ".AO = clamp(" + In(n, "Ambient Occlusion", T::Float) + ", 0.0, 1.0);");
					m_PartialAlpha |= PinActive(n, "Transparency", 0.001f, true);
					return;
				}
				if (t == HoldoutShader) {
					const std::string b = closure("Holdout");
					Line(b + ".BaseColor = vec3(0.0); " + b + ".Specular = 0.0; " + b + ".Roughness = 1.0;");
					return;
				}
				if (t == PrincipledVolume || t == VolumeAbsorption || t == VolumeScatter) {
					// No volumetrics in the raster engine: plugged into a surface it shows its color
					const std::string b = closure("Volume");
					Line(b + ".BaseColor = " + In(n, "Color", T::Vec3) + ";");
					return;
				}
				if (t == ShaderToRGB) {
					const std::string c = "n" + std::to_string(n.NodeID) + "_s2r";
					Line("vec4 " + c + " = lnx_shader_to_rgb(" + In(n, "Shader", T::BSDF) + ");");
					Decl(n, "Color", T::Vec4, "vec4(" + c + ".rgb, 1.0)");
					Decl(n, "Alpha", T::Float, c + ".a");
					m_ShaderToRGB = true;
					return;
				}

				// ---------------- Input ----------------
				if (t == ValueNode || t == FloatNode) {
					const int slot = AddParam({ n.NodeID, 0 });
					RegisterName(n, slot);
					Decl(n, "Value", T::Float, ParamExpr(slot, T::Float));
					return;
				}
				if (t == RGBNode || t == ColorNode) {
					const int slot = AddParam({ n.NodeID, 0 });
					RegisterName(n, slot);
					Decl(n, "Color", T::Vec4, ParamExpr(slot, T::Vec4));
					return;
				}
				if (t == FresnelNode) {
					Decl(n, "Fac", T::Float, "lnx_fresnel(" + In(n, "IOR", T::Float) + ", " + In(n, "Normal", T::Vec3, "LNX_NORMAL") + ")");
					return;
				}
				if (t == LayerWeight) {
					const std::string blend = In(n, "Blend", T::Float), N = In(n, "Normal", T::Vec3, "LNX_NORMAL");
					Decl(n, "Fresnel", T::Float, "lnx_fresnel(1.0 / max(1.0 - clamp(" + blend + ", 0.0, 1.0), 1e-5), " + N + ")");
					Decl(n, "Facing", T::Float, "lnx_facing(" + blend + ", " + N + ")");
					return;
				}
				if (t == TimeNode) {
					Decl(n, "Seconds", T::Float, "u_GraphParams[0].x");
					return;
				}

				// ---------------- Texture ----------------
				if (t == ImageTexture || t == TextureNode) {
					const bool legacy = t == TextureNode;
					const std::string uv = legacy ? std::string("vec3(LNX_UV, 0.0)") : In(n, "Vector", T::Vec3, "vec3(LNX_UV, 0.0)");
					const std::string path = Prop(n, "Path");
					std::string sample = "vec4(1.0, 0.0, 1.0, 1.0)";   // missing texture: magenta (like Blender)
					if (!path.empty()) {
						const int tex = AddTexture(path);
						if (tex >= 0) {
							const std::string s = "u_GraphTex" + std::to_string(tex);
							if (legacy) {
								sample = "texture(" + s + ", (" + uv + ").xy)";
							}
							else {
								std::string space = Prop(n, "Color Space");
								if (space.empty()) space = GuessColorSpace(path);
								const int cs = space == "sRGB" ? 0 : space == "Linear Rec.709" ? 1 : 2;
								sample = "lnx_image_post(lnx_image(" + s + ", " + uv + ", " + std::to_string(Op(n, "Projection")) + ", " + Lit(n.Value.x) + ", " +
									std::to_string(Op(n, "Extension")) + ", " + std::to_string(Op(n, "Interpolation")) + "), " +
									std::to_string(cs) + ", " + std::to_string(Op(n, "Alpha")) + ")";
							}
						}
					}
					Line("vec4 " + id + "_s = " + sample + ";");
					if (legacy) {
						Decl(n, "RGB", T::Vec4, id + "_s");
						Decl(n, "R", T::Float, id + "_s.r");
						Decl(n, "G", T::Float, id + "_s.g");
						Decl(n, "B", T::Float, id + "_s.b");
						Decl(n, "A", T::Float, id + "_s.a");
					}
					else {
						Decl(n, "Color", T::Vec4, id + "_s");
						Decl(n, "Alpha", T::Float, id + "_s.a");
					}
					return;
				}
				if (t == CheckerTexture) {
					Line("float " + id + "_f;");
					Line("vec4 " + id + "_c = lnx_checker(" + In(n, "Vector", T::Vec3, "LNX_POS") + ", " + In(n, "Color1", T::Vec4) + ", " + In(n, "Color2", T::Vec4) + ", " +
						In(n, "Scale", T::Float) + ", " + id + "_f);");
					Decl(n, "Color", T::Vec4, id + "_c");
					Decl(n, "Fac", T::Float, id + "_f");
					return;
				}
				// ---------------- Color ----------------
				if (t == MixNode) {
					const std::string type = Prop(n, "Data Type", "Color");
					const std::string f = In(n, "Factor", T::Float);
					if (type == "Float") {
						std::string r = "mix(" + In(n, "A Float", T::Float) + ", " + In(n, "B Float", T::Float) + ", clamp(" + f + ", 0.0, 1.0))";
						if (Toggle(n, "Clamp Result")) r = "clamp(" + r + ", 0.0, 1.0)";
						Decl(n, "Result Float", T::Float, r);
						Decl(n, "Result", T::Vec4, "vec4(vec3(" + Var(n, "Result Float") + "), 1.0)");
						Decl(n, "Result Vector", T::Vec3, "vec3(" + Var(n, "Result Float") + ")");
					}
					else if (type == "Vector") {
						Decl(n, "Result Vector", T::Vec3, "mix(" + In(n, "A Vector", T::Vec3) + ", " + In(n, "B Vector", T::Vec3) + ", clamp(" + f + ", 0.0, 1.0))");
						Decl(n, "Result", T::Vec4, "vec4(" + Var(n, "Result Vector") + ", 1.0)");
						Decl(n, "Result Float", T::Float, "lnx_avg(" + Var(n, "Result Vector") + ")");
					}
					else {
						Decl(n, "Result", T::Vec4, "lnx_blend(" + std::to_string(Op(n, "Blending")) + ", " + f + ", " + In(n, "A", T::Vec4) + ", " + In(n, "B", T::Vec4) + ", " +
							(Toggle(n, "Clamp Result") ? "true" : "false") + ")");
						Decl(n, "Result Float", T::Float, "lnx_luma(" + Var(n, "Result") + ".rgb)");
						Decl(n, "Result Vector", T::Vec3, Var(n, "Result") + ".rgb");
					}
					return;
				}
				if (t == InvertNode) {
					const std::string c = In(n, "Color", T::Vec4);
					Line("vec4 " + id + "_c = " + c + ";");
					Decl(n, "Color", T::Vec4, "vec4(mix(" + id + "_c.rgb, 1.0 - " + id + "_c.rgb, " + In(n, "Fac", T::Float) + "), " + id + "_c.a)");
					return;
				}
				if (t == HueSatValNode) {
					Line("vec4 " + id + "_c = " + In(n, "Color", T::Vec4) + ";");
					Line("vec3 " + id + "_h = lnx_rgb2hsv(" + id + "_c.rgb);");
					Line(id + "_h.x = fract(" + id + "_h.x + " + In(n, "Hue", T::Float) + " - 0.5);");
					Line(id + "_h.y = clamp(" + id + "_h.y * " + In(n, "Saturation", T::Float) + ", 0.0, 1.0);");
					Line(id + "_h.z *= " + In(n, "Value", T::Float) + ";");
					Decl(n, "Color", T::Vec4, "vec4(mix(" + id + "_c.rgb, lnx_hsv2rgb(" + id + "_h), " + In(n, "Fac", T::Float) + "), " + id + "_c.a)");
					return;
				}
				if (t == BrightContrast) {
					const std::string contrast = In(n, "Contrast", T::Float);
					Line("vec4 " + id + "_c = " + In(n, "Color", T::Vec4) + ";");
					Decl(n, "Color", T::Vec4, "vec4(max((1.0 + " + contrast + ") * " + id + "_c.rgb + (" + In(n, "Bright", T::Float) + " - " + contrast + " * 0.5), vec3(0.0)), " + id + "_c.a)");
					return;
				}
				if (t == GammaNode) {
					Line("vec4 " + id + "_c = " + In(n, "Color", T::Vec4) + ";");
					Decl(n, "Color", T::Vec4, "vec4(pow(max(" + id + "_c.rgb, vec3(0.0)), vec3(" + In(n, "Gamma", T::Float) + ")), " + id + "_c.a)");
					return;
				}

				// ---------------- Vector ----------------
				if (t == MappingNode) {
					Decl(n, "Vector", T::Vec3, "lnx_mapping(" + In(n, "Vector", T::Vec3, "vec3(LNX_UV, 0.0)") + ", " + In(n, "Location", T::Vec3) + ", " +
						In(n, "Rotation", T::Vec3) + ", " + In(n, "Scale", T::Vec3) + ", " + std::to_string(Op(n, "Type")) + ")");
					return;
				}
				if (t == NormalMapNode) {
					Decl(n, "Normal", T::Vec3, "lnx_normal_map(" + In(n, "Color", T::Vec4) + ", " + In(n, "Strength", T::Float) + ")");
					return;
				}
				if (t == BumpNode) {
					Decl(n, "Normal", T::Vec3, "lnx_bump(" + In(n, "Strength", T::Float) + ", " + In(n, "Distance", T::Float) + ", " + In(n, "Height", T::Float) + ", " +
						"normalize(" + In(n, "Normal", T::Vec3, "LNX_NORMAL") + "), " + (Toggle(n, "Invert") ? "true" : "false") + ")");
					return;
				}
				if (t == VectorMathNode) {
					const int op = Op(n, "Operation");
					const std::string a = In(n, "A", T::Vec3);
					const std::string b = Visible(n, "B") ? In(n, "B", T::Vec3) : "vec3(0.0)";
					const std::string c = Visible(n, "C") ? In(n, "C", T::Vec3) : "vec3(0.0)";
					const std::string s = Visible(n, "Scale") ? In(n, "Scale", T::Float) : "1.0";
					Decl(n, "Vector", T::Vec3, "lnx_vmath(" + std::to_string(op) + ", " + a + ", " + b + ", " + c + ", " + s + ")");
					Decl(n, "Value", T::Float, "lnx_vmath_value(" + std::to_string(op) + ", " + a + ", " + b + ")");
					return;
				}

				// ---------------- Converter ----------------
				if (t == MathNode) {
					const std::string a = In(n, "A", T::Float);
					const std::string b = Visible(n, "B") ? In(n, "B", T::Float) : "0.0";
					const std::string c = Visible(n, "C") ? In(n, "C", T::Float) : "0.0";
					std::string r = "lnx_math(" + std::to_string(Op(n, "Operation")) + ", " + a + ", " + b + ", " + c + ")";
					if (Toggle(n, "Clamp")) r = "clamp(" + r + ", 0.0, 1.0)";
					Decl(n, "Value", T::Float, r);
					return;
				}
				if (t == MapRangeNode) {
					const int interp = Op(n, "Interpolation");
					const std::string steps = interp == 1 ? In(n, "Steps", T::Float) : "4.0";
					Decl(n, "Result", T::Float, "lnx_map_range(" + In(n, "Value", T::Float) + ", " + In(n, "From Min", T::Float) + ", " + In(n, "From Max", T::Float) + ", " +
						In(n, "To Min", T::Float) + ", " + In(n, "To Max", T::Float) + ", " + steps + ", " + std::to_string(interp) + ", " +
						(Toggle(n, "Clamp") ? "true" : "false") + ")");
					return;
				}
				if (t == ClampNode) {
					const std::string mn = In(n, "Min", T::Float), mx = In(n, "Max", T::Float);
					const std::string v = In(n, "Value", T::Float);
					Decl(n, "Result", T::Float, Op(n, "Clamp Type") == 1
						? "clamp(" + v + ", min(" + mn + ", " + mx + "), max(" + mn + ", " + mx + "))"
						: "clamp(" + v + ", " + mn + ", " + mx + ")");
					return;
				}
				if (t == ColorRampNode) {
					const auto stops = ParseRamp(Prop(n, "Stops"));
					const int interp = Op(n, "Interpolation");
					Line("float " + id + "_t = clamp(" + In(n, "Fac", T::Float) + ", 0.0, 1.0);");
					std::vector<int> colorSlots, posSlots;
					for (int i = 0; i < static_cast<int>(stops.size()); i++) {
						ParamRef c{ n.NodeID, 0, i, false }, p{ n.NodeID, 0, i, true };
						colorSlots.push_back(AddParam(c));
						posSlots.push_back(AddParam(p));
					}
					Line("vec4 " + id + "_c = " + ParamExpr(colorSlots[0], T::Vec4) + ";");
					for (size_t i = 1; i < stops.size(); i++) {
						const std::string p0 = ParamExpr(posSlots[i - 1], T::Float), p1 = ParamExpr(posSlots[i], T::Float);
						std::string w;
						if (interp == 2) w = "step(" + p1 + ", " + id + "_t)";
						else {
							w = "clamp((" + id + "_t - " + p0 + ") / max(" + p1 + " - " + p0 + ", 1e-5), 0.0, 1.0)";
							if (interp == 1) w = "smoothstep(0.0, 1.0, " + w + ")";
						}
						Line(id + "_c = mix(" + id + "_c, " + ParamExpr(colorSlots[i], T::Vec4) + ", " + w + ");");
					}
					Decl(n, "Color", T::Vec4, id + "_c");
					Decl(n, "Alpha", T::Float, id + "_c.a");
					return;
				}
				if (t == SeparateColor) {
					Line("vec4 " + id + "_c = " + In(n, "Color", T::Vec4) + ";");
					if (Prop(n, "Mode", "RGB") == "HSV") Line(id + "_c.rgb = lnx_rgb2hsv(" + id + "_c.rgb);");
					Decl(n, "Red", T::Float, id + "_c.r");
					Decl(n, "Green", T::Float, id + "_c.g");
					Decl(n, "Blue", T::Float, id + "_c.b");
					Decl(n, "Alpha", T::Float, id + "_c.a");
					return;
				}
				if (t == CombineColor) {
					std::string rgb = "vec3(" + In(n, "Red", T::Float) + ", " + In(n, "Green", T::Float) + ", " + In(n, "Blue", T::Float) + ")";
					if (Prop(n, "Mode", "RGB") == "HSV") rgb = "lnx_hsv2rgb(" + rgb + ")";
					Decl(n, "Color", T::Vec4, "vec4(" + rgb + ", 1.0)");
					return;
				}
				if (t == SeparateXYZ) {
					Line("vec3 " + id + "_v = " + In(n, "Vector", T::Vec3) + ";");
					Decl(n, "X", T::Float, id + "_v.x");
					Decl(n, "Y", T::Float, id + "_v.y");
					Decl(n, "Z", T::Float, id + "_v.z");
					return;
				}
				if (t == CombineXYZ) {
					Decl(n, "Vector", T::Vec3, "vec3(" + In(n, "X", T::Float) + ", " + In(n, "Y", T::Float) + ", " + In(n, "Z", T::Float) + ")");
					return;
				}
				if (t == RGBToBW) {
					Decl(n, "Val", T::Float, "lnx_luma(" + In(n, "Color", T::Vec3) + ")");
					return;
				}

				// ---------------- Input (Blender 4.x) ----------------
				if (t == TexCoordNode) {
					Decl(n, "Generated", T::Vec3, "LNX_OBJPOS");
					Decl(n, "Normal", T::Vec3, "normalize(transpose(mat3(LNX_MODEL)) * LNX_NORMAL)");
					Decl(n, "UV", T::Vec3, "vec3(LNX_UV, 0.0)");
					Decl(n, "Object", T::Vec3, "LNX_OBJPOS");
					Decl(n, "Camera", T::Vec3, "(LNX_VIEW * vec4(LNX_POS, 1.0)).xyz");
					Line("vec4 " + id + "_clip = LNX_VIEWPROJ * vec4(LNX_POS, 1.0);");
					Decl(n, "Window", T::Vec3, "vec3(" + id + "_clip.xy / max(" + id + "_clip.w, 1e-6) * 0.5 + 0.5, 0.0)");
					Decl(n, "Reflection", T::Vec3, "reflect(-lnx_view_dir(), LNX_NORMAL)");
					return;
				}
				if (t == GeometryNode) {
					Decl(n, "Position", T::Vec3, "LNX_POS");
					Decl(n, "Normal", T::Vec3, "LNX_NORMAL");
					Decl(n, "Tangent", T::Vec3, "normalize(LNX_TBN[0])");
					Line("vec3 " + id + "_tn = normalize(cross(dFdx(LNX_POS), dFdy(LNX_POS)));");
					Decl(n, "True Normal", T::Vec3, "(dot(" + id + "_tn, LNX_NORMAL) < 0.0 ? -" + id + "_tn : " + id + "_tn)");
					Decl(n, "Incoming", T::Vec3, "lnx_view_dir()");
					Decl(n, "Parametric", T::Vec3, "vec3(fract(LNX_UV), 0.0)");
					Decl(n, "Backfacing", T::Float, "(gl_FrontFacing ? 0.0 : 1.0)");
					Decl(n, "Pointiness", T::Float, "0.5");
					Decl(n, "Random Per Island", T::Float, "0.0");
					return;
				}
				if (t == AmbientOcclusionNode) {
					Decl(n, "Color", T::Vec4, In(n, "Color", T::Vec4));
					Decl(n, "AO", T::Float, "1.0");
					return;
				}
				if (t == AttributeNode) {
					std::string name = Prop(n, "Name");
					for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
					std::string v = "vec3(0.0)";
					if (name == "uvmap" || name == "uv" || name == "uv0" || name == "texcoord") v = "vec3(LNX_UV, 0.0)";
					else if (name == "position" || name == "p") v = "LNX_POS";
					else if (name == "normal" || name == "n") v = "LNX_NORMAL";
					Decl(n, "Vector", T::Vec3, v);
					Decl(n, "Color", T::Vec4, "vec4(" + Var(n, "Vector") + ", 1.0)");
					Decl(n, "Fac", T::Float, "lnx_avg(" + Var(n, "Vector") + ")");
					Decl(n, "Alpha", T::Float, "1.0");
					return;
				}
				if (t == BevelNode) {
					Decl(n, "Normal", T::Vec3, "normalize(" + In(n, "Normal", T::Vec3, "LNX_NORMAL") + ")");
					return;
				}
				if (t == CameraDataNode) {
					Line("vec3 " + id + "_v = (LNX_VIEW * vec4(LNX_POS, 1.0)).xyz;");
					Decl(n, "View Vector", T::Vec3, "normalize(" + id + "_v)");
					Decl(n, "View Z Depth", T::Float, "-" + id + "_v.z");
					Decl(n, "View Distance", T::Float, "length(" + id + "_v)");
					return;
				}
				if (t == ColorAttributeNode) {
					Decl(n, "Color", T::Vec4, "vec4(1.0)");
					Decl(n, "Alpha", T::Float, "1.0");
					return;
				}
				if (t == LightPathNode) {
					for (const auto& o : n.Outputs) {
						std::string v = "0.0";
						if (o.Name == "Is Camera Ray") v = "1.0";
						else if (o.Name == "Ray Length") v = "length(LNX_POS - LNX_VIEWPOS)";
						Decl(n, o.Name, T::Float, v);
					}
					return;
				}
				if (t == ObjectInfoNode) {
					Decl(n, "Location", T::Vec3, "LNX_MODEL[3].xyz");
					Decl(n, "Color", T::Vec4, "vec4(1.0)");
					Decl(n, "Alpha", T::Float, "1.0");
					Decl(n, "Object Index", T::Float, "LNX_OBJECT_ID");
					Decl(n, "Material Index", T::Float, "0.0");
					Decl(n, "Random", T::Float, "lnx_hash14(vec4(LNX_OBJECT_ID, LNX_MODEL[3].xyz))");
					return;
				}
				if (t == CurvesInfoNode || t == ParticleInfoNode || t == PointInfoNode || t == VolumeInfoNode) {
					for (const auto& o : n.Outputs) Decl(n, o.Name, TypeOf(o.Type), Conv("0.0", T::Float, TypeOf(o.Type)));
					return;
				}
				if (t == TangentNode) {
					if (Prop(n, "Direction Type", "UV Map") == "UV Map") Decl(n, "Tangent", T::Vec3, "normalize(LNX_TBN[0])");
					else {
						const int axis = Op(n, "Axis");
						const std::string a = axis == 0 ? "vec3(1.0, 0.0, 0.0)" : axis == 1 ? "vec3(0.0, 1.0, 0.0)" : "vec3(0.0, 0.0, 1.0)";
						Decl(n, "Tangent", T::Vec3, "normalize(cross(mat3(LNX_MODEL) * " + a + ", LNX_POS - LNX_MODEL[3].xyz) + vec3(1e-6))");
					}
					return;
				}
				if (t == UVMapNode) { Decl(n, "UV", T::Vec3, "vec3(LNX_UV, 0.0)"); return; }
				if (t == WireframeNode) { Decl(n, "Fac", T::Float, "0.0"); return; }
				if (t == AOVOutput) return;

				// ---------------- Texture (Blender 4.x) ----------------
				if (t == NoiseTexture) {
					const int dims = std::max(1, Op(n, "Dimensions") + 1);
					const int type = Op(n, "Type");
					const std::string v = Visible(n, "Vector") ? In(n, "Vector", T::Vec3, "LNX_OBJPOS") : "vec3(0.0)";
					const std::string w = Visible(n, "W") ? In(n, "W", T::Float) : "0.0";
					const std::string offset = Visible(n, "Offset") ? In(n, "Offset", T::Float) : "0.0";
					const std::string gain = Visible(n, "Gain") ? In(n, "Gain", T::Float) : "1.0";
					Line("vec4 " + id + "_n = lnx_noise4(" + v + ", " + w + ", " + std::to_string(dims) + ", " + In(n, "Scale", T::Float) + ", " +
						In(n, "Detail", T::Float) + ", " + In(n, "Roughness", T::Float) + ", " + In(n, "Lacunarity", T::Float) + ", " + offset + ", " + gain + ", " +
						In(n, "Distortion", T::Float) + ", " + std::to_string(type) + ", " + (Prop(n, "Normalize", "1") == "1" ? "true" : "false") + ");");
					Decl(n, "Fac", T::Float, id + "_n.w");
					Decl(n, "Color", T::Vec4, "vec4(" + id + "_n.rgb, 1.0)");
					return;
				}
				if (t == VoronoiTexture) {
					const int dims = std::max(1, Op(n, "Dimensions") + 1);
					const std::string v = Visible(n, "Vector") ? In(n, "Vector", T::Vec3, "LNX_OBJPOS") : "vec3(0.0)";
					const std::string w = Visible(n, "W") ? In(n, "W", T::Float) : "0.0";
					Line("float " + id + "_d; vec3 " + id + "_c; vec3 " + id + "_p; float " + id + "_w; float " + id + "_r;");
					Line("lnx_voronoi4(" + v + ", " + w + ", " + std::to_string(dims) + ", " + In(n, "Scale", T::Float) + ", " + In(n, "Detail", T::Float) + ", " +
						In(n, "Roughness", T::Float) + ", " + In(n, "Lacunarity", T::Float) + ", " + In(n, "Smoothness", T::Float) + ", " + In(n, "Exponent", T::Float) + ", " +
						In(n, "Randomness", T::Float) + ", " + std::to_string(Op(n, "Feature")) + ", " + std::to_string(Op(n, "Distance")) + ", " +
						(Prop(n, "Normalize", "0") == "1" ? "true" : "false") + ", " + id + "_d, " + id + "_c, " + id + "_p, " + id + "_w, " + id + "_r);");
					Decl(n, "Distance", T::Float, id + "_d");
					Decl(n, "Color", T::Vec4, "vec4(" + id + "_c, 1.0)");
					Decl(n, "Position", T::Vec3, id + "_p");
					Decl(n, "W", T::Float, id + "_w");
					Decl(n, "Radius", T::Float, id + "_r");
					return;
				}
				if (t == WaveTexture) {
					Line("float " + id + "_w = lnx_wave4(" + In(n, "Vector", T::Vec3, "LNX_OBJPOS") + ", " + In(n, "Scale", T::Float) + ", " + In(n, "Distortion", T::Float) + ", " +
						In(n, "Detail", T::Float) + ", " + In(n, "Detail Scale", T::Float) + ", " + In(n, "Detail Roughness", T::Float) + ", " + In(n, "Phase Offset", T::Float) + ", " +
						std::to_string(Op(n, "Type")) + ", " + std::to_string(Op(n, "Direction")) + ", " + std::to_string(Op(n, "Profile")) + ");");
					Decl(n, "Color", T::Vec4, "vec4(vec3(" + id + "_w), 1.0)");
					Decl(n, "Fac", T::Float, id + "_w");
					return;
				}
				if (t == GradientTexture) {
					Line("float " + id + "_g = lnx_gradient4(" + In(n, "Vector", T::Vec3, "LNX_OBJPOS") + ", " + std::to_string(Op(n, "Type")) + ");");
					Decl(n, "Color", T::Vec4, "vec4(vec3(" + id + "_g), 1.0)");
					Decl(n, "Fac", T::Float, id + "_g");
					return;
				}
				if (t == BrickTexture) {
					auto num = [&](const char* key, const char* fallback) { return Lit(static_cast<float>(std::atof(Prop(n, key, fallback).c_str()))); };
					auto inum = [&](const char* key, const char* fallback) { return std::to_string(std::max(0, std::atoi(Prop(n, key, fallback).c_str()))); };
					Line("vec2 " + id + "_b = lnx_brick(" + In(n, "Vector", T::Vec3, "LNX_OBJPOS") + " * " + In(n, "Scale", T::Float) + ", " + In(n, "Mortar Size", T::Float) + ", " +
						In(n, "Mortar Smooth", T::Float) + ", " + In(n, "Bias", T::Float) + ", " + In(n, "Brick Width", T::Float) + ", " + In(n, "Row Height", T::Float) + ", " +
						num("Offset", "0.5") + ", " + inum("Offset Frequency", "2") + ", " + num("Squash", "1.0") + ", " + inum("Squash Frequency", "2") + ");");
					Line("vec4 " + id + "_c1 = " + In(n, "Color1", T::Vec4) + ";");
					Line("if (" + id + "_b.y != 1.0) " + id + "_c1 = mix(" + id + "_c1, " + In(n, "Color2", T::Vec4) + ", " + id + "_b.x);");
					Decl(n, "Color", T::Vec4, "mix(" + id + "_c1, " + In(n, "Mortar", T::Vec4) + ", " + id + "_b.y)");
					Decl(n, "Fac", T::Float, id + "_b.y");
					return;
				}
				if (t == EnvironmentTexture) {
					const std::string path = Prop(n, "Path");
					const std::string dir = In(n, "Vector", T::Vec3, "-lnx_view_dir()");
					std::string sample = "vec4(1.0, 0.0, 1.0, 1.0)";
					if (!path.empty()) {
						const int tex = AddTexture(path);
						if (tex >= 0) {
							std::string space = Prop(n, "Color Space");
							if (space.empty()) space = "sRGB";
							const int cs = space == "sRGB" ? 0 : space == "Linear Rec.709" ? 1 : 2;
							sample = "lnx_image_post(lnx_image_sample(u_GraphTex" + std::to_string(tex) + ", lnx_env_uv(" + dir + ", " + std::to_string(Op(n, "Projection")) + "), 0, " +
								std::to_string(Op(n, "Interpolation")) + "), " + std::to_string(cs) + ", 0)";
						}
					}
					Decl(n, "Color", T::Vec4, sample);
					return;
				}
				if (t == GaborTexture) {
					const bool is3D = Prop(n, "Type", "2D") == "3D";
					const std::string orient = is3D ? In(n, "Orientation 3D", T::Vec3)
						: "vec3(cos(" + In(n, "Orientation", T::Float) + "), sin(" + In(n, "Orientation", T::Float) + "), 0.0)";
					Line("vec3 " + id + "_g = lnx_gabor(" + In(n, "Vector", T::Vec3, "LNX_OBJPOS") + ", " + In(n, "Scale", T::Float) + ", " + In(n, "Frequency", T::Float) + ", " +
						In(n, "Anisotropy", T::Float) + ", " + orient + ", " + (is3D ? "true" : "false") + ");");
					Decl(n, "Value", T::Float, id + "_g.x");
					Decl(n, "Phase", T::Float, id + "_g.y");
					Decl(n, "Intensity", T::Float, id + "_g.z");
					return;
				}
				if (t == IESTexture) { Decl(n, "Fac", T::Float, In(n, "Strength", T::Float)); return; }
				if (t == MagicTexture) {
					Line("vec4 " + id + "_m = lnx_magic(" + In(n, "Vector", T::Vec3, "LNX_OBJPOS") + ", " + In(n, "Scale", T::Float) + ", " + In(n, "Distortion", T::Float) + ", " +
						std::to_string(std::clamp(std::atoi(Prop(n, "Depth", "2").c_str()), 0, 10)) + ");");
					Decl(n, "Color", T::Vec4, "vec4(" + id + "_m.rgb, 1.0)");
					Decl(n, "Fac", T::Float, id + "_m.w");
					return;
				}
				if (t == PointDensityTexture) {
					Decl(n, "Color", T::Vec4, "vec4(0.0, 0.0, 0.0, 1.0)");
					Decl(n, "Density", T::Float, "0.0");
					return;
				}
				if (t == SkyTexture) {
					Decl(n, "Color", T::Vec4, "vec4(lnx_sky(" + In(n, "Vector", T::Vec3, "-lnx_view_dir()") + ", " + In(n, "Sun Size", T::Float) + ", " + In(n, "Sun Intensity", T::Float) + ", " +
						In(n, "Sun Elevation", T::Float) + ", " + In(n, "Sun Rotation", T::Float) + ", " + In(n, "Air", T::Float) + ", " + In(n, "Dust", T::Float) + ", " +
						In(n, "Ozone", T::Float) + ", " + (Prop(n, "Sun Disc", "1") == "1" ? "true" : "false") + "), 1.0)");
					return;
				}
				if (t == WhiteNoiseTexture) {
					const int dims = Op(n, "Dimensions") + 1;
					const std::string v = Visible(n, "Vector") ? In(n, "Vector", T::Vec3, "LNX_OBJPOS") : "vec3(0.0)";
					const std::string w = Visible(n, "W") ? In(n, "W", T::Float) : "0.0";
					std::string p;
					if (dims == 1) p = "vec4(" + w + ", 0.0, 0.0, 0.0)";
					else if (dims == 2) p = "vec4((" + v + ").xy, 0.0, 0.0)";
					else if (dims == 3) p = "vec4(" + v + ", 0.0)";
					else p = "vec4(" + v + ", " + w + ")";
					Line("vec4 " + id + "_p = " + p + ";");
					Decl(n, "Value", T::Float, "lnx_hash14(" + id + "_p)");
					Decl(n, "Color", T::Vec4, "vec4(lnx_hash14(" + id + "_p + vec4(1.0, 0.0, 0.0, 0.0)), lnx_hash14(" + id + "_p + vec4(0.0, 2.0, 0.0, 0.0)), lnx_hash14(" + id + "_p + vec4(0.0, 0.0, 3.0, 0.0)), 1.0)");
					return;
				}

				// ---------------- Color (Blender 4.x) ----------------
				if (t == LightFalloffNode) {
					Line("float " + id + "_r = length(LNX_POS - LNX_VIEWPOS);");
					Line("float " + id + "_s = " + In(n, "Strength", T::Float) + ";");
					Line("{ float sm = " + In(n, "Smooth", T::Float) + "; if (sm > 0.0) " + id + "_s *= (" + id + "_r * " + id + "_r) / (sm + " + id + "_r * " + id + "_r); }");
					Decl(n, "Quadratic", T::Float, id + "_s");
					Decl(n, "Linear", T::Float, id + "_s * " + id + "_r");
					Decl(n, "Constant", T::Float, id + "_s * " + id + "_r * " + id + "_r");
					return;
				}
				if (t == RGBCurvesNode) {
					Line("vec4 " + id + "_in = " + In(n, "Color", T::Vec4) + ";");
					Line("vec3 " + id + "_o = " + id + "_in.rgb;");
					const char* ch[3] = { "Curve R", "Curve G", "Curve B" };
					const char* sw[3] = { "r", "g", "b" };
					for (int i = 0; i < 3; i++) {
						const std::string c = Curve(n, "Curve C", id + "_in." + sw[i]);
						Line(id + "_o." + sw[i] + " = " + Curve(n, ch[i], c) + ";");
					}
					Decl(n, "Color", T::Vec4, "vec4(mix(" + id + "_in.rgb, " + id + "_o, clamp(" + In(n, "Fac", T::Float) + ", 0.0, 1.0)), " + id + "_in.a)");
					return;
				}

				// ---------------- Vector (Blender 4.x) ----------------
				if (t == DisplacementNode) {
					std::string N = In(n, "Normal", T::Vec3, "LNX_NORMAL");
					Decl(n, "Displacement", T::Vec3, "(" + In(n, "Height", T::Float) + " - " + In(n, "Midlevel", T::Float) + ") * " + In(n, "Scale", T::Float) + " * normalize(" + N + ")");
					return;
				}
				if (t == VectorDisplacementNode) {
					Line("vec3 " + id + "_v = (" + In(n, "Vector", T::Vec3) + " - vec3(" + In(n, "Midlevel", T::Float) + ")) * " + In(n, "Scale", T::Float) + ";");
					const int space = Op(n, "Space");
					if (space == 0) Decl(n, "Displacement", T::Vec3, "LNX_TBN * " + id + "_v");
					else if (space == 1) Decl(n, "Displacement", T::Vec3, "mat3(LNX_MODEL) * " + id + "_v");
					else Decl(n, "Displacement", T::Vec3, id + "_v");
					return;
				}
				if (t == NormalNode) {
					const int slot = AddParam({ n.NodeID, 0 });
					Line("vec3 " + id + "_dir = normalize(" + ParamExpr(slot, T::Vec3) + " + vec3(0.0, 0.0, 1e-6));");
					Decl(n, "Normal", T::Vec3, id + "_dir");
					Decl(n, "Dot", T::Float, "dot(" + In(n, "Normal", T::Vec3) + ", " + id + "_dir)");
					return;
				}
				if (t == VectorCurvesNode) {
					Line("vec3 " + id + "_in = " + In(n, "Vector", T::Vec3) + ";");
					const std::string x = Curve(n, "Curve X", id + "_in.x");
					const std::string y = Curve(n, "Curve Y", id + "_in.y");
					const std::string z = Curve(n, "Curve Z", id + "_in.z");
					Decl(n, "Vector", T::Vec3, "mix(" + id + "_in, vec3(" + x + ", " + y + ", " + z + "), clamp(" + In(n, "Fac", T::Float) + ", 0.0, 1.0))");
					return;
				}
				if (t == VectorRotateNode) {
					const int type = Op(n, "Type");
					const bool inv = Prop(n, "Invert", "0") == "1";
					Line("vec3 " + id + "_c = " + In(n, "Center", T::Vec3) + ";");
					Line("vec3 " + id + "_v = " + In(n, "Vector", T::Vec3) + " - " + id + "_c;");
					std::string r;
					if (type == 4) {
						Line("mat3 " + id + "_m = lnx_euler(" + In(n, "Rotation", T::Vec3) + ");");
						r = (inv ? "transpose(" + id + "_m)" : id + "_m") + " * " + id + "_v";
					}
					else {
						const std::string axis = type == 0 ? In(n, "Axis", T::Vec3) : type == 1 ? "vec3(1.0, 0.0, 0.0)" : type == 2 ? "vec3(0.0, 1.0, 0.0)" : "vec3(0.0, 0.0, 1.0)";
						r = "lnx_rotate_axis(" + id + "_v, " + axis + ", " + std::string(inv ? "-" : "") + In(n, "Angle", T::Float) + ")";
					}
					Decl(n, "Vector", T::Vec3, r + " + " + id + "_c");
					return;
				}
				if (t == VectorTransformNode) {
					const int type = Op(n, "Type"), from = Op(n, "Convert From"), to = Op(n, "Convert To");
					Line("vec3 " + id + "_v = " + In(n, "Vector", T::Vec3) + ";");
					const std::string w = type == 0 ? "1.0" : "0.0";
					// to world
					if (from == 1) Line(id + "_v = " + std::string(type == 2 ? "normalize(mat3(transpose(LNX_MODEL_INV)) * " + id + "_v)" : "(LNX_MODEL * vec4(" + id + "_v, " + w + ")).xyz") + ";");
					else if (from == 2) Line(id + "_v = (inverse(LNX_VIEW) * vec4(" + id + "_v, " + w + ")).xyz;");
					// world to target
					if (to == 1) Line(id + "_v = " + std::string(type == 2 ? "normalize(mat3(transpose(LNX_MODEL)) * " + id + "_v)" : "(LNX_MODEL_INV * vec4(" + id + "_v, " + w + ")).xyz") + ";");
					else if (to == 2) Line(id + "_v = (LNX_VIEW * vec4(" + id + "_v, " + w + ")).xyz;");
					Decl(n, "Vector", T::Vec3, id + "_v");
					return;
				}

				// ---------------- Converter (Blender 4.x) ----------------
				if (t == BlackbodyNode) { Decl(n, "Color", T::Vec4, "vec4(lnx_blackbody(" + In(n, "Temperature", T::Float) + "), 1.0)"); return; }
				if (t == WavelengthNode) { Decl(n, "Color", T::Vec4, "vec4(lnx_wavelength(" + In(n, "Wavelength", T::Float) + "), 1.0)"); return; }
				if (t == FloatCurveNode) {
					Line("float " + id + "_in = " + In(n, "Value", T::Float) + ";");
					const std::string v = Curve(n, "Curve V", id + "_in");
					Decl(n, "Value", T::Float, "mix(" + id + "_in, " + v + ", clamp(" + In(n, "Factor", T::Float) + ", 0.0, 1.0))");
					return;
				}

				// ---------------- Legacy ----------------
				if (t == MultiplyNode) {
					Decl(n, "Result", T::Vec4, In(n, "A", T::Vec4) + " * " + In(n, "B", T::Vec4));
					return;
				}

				m_Out.Errors.push_back("Node type not supported by the shader compiler: " + n.Title);
				for (const auto& out : n.Outputs) Decl(n, out.Name, TypeOf(out.Type), Conv("0.0", T::Float, TypeOf(out.Type)));
			}

			// Inlines a node group: its inputs become variables, the subgraph is generated in place
			void EmitGroup(const Node& n) {
				if (!n.Subgraph) {
					for (const auto& o : n.Outputs) Decl(n, o.Name, TypeOf(o.Type), Conv("0.0", T::Float, TypeOf(o.Type)));
					return;
				}
				Bindings bindings;
				for (size_t i = 0; i < n.Inputs.size(); i++) {
					const Pin& in = n.Inputs[i];
					const T type = TypeOf(in.Type);
					const std::string var = "n" + std::to_string(n.NodeID) + "_in" + std::to_string(i);
					Line(std::string(GL(type)) + " " + var + " = " + In(n, in.Name.c_str(), type) + ";");
					bindings[in.Name] = { var, type };
				}
				Generator child(*n.Subgraph, m_Out, m_Body, &bindings);
				const Node* groupOut = n.Subgraph->FindNodeByType(GroupOutputType);
				for (const auto& o : n.Outputs) {
					const T type = TypeOf(o.Type);
					const std::string expr = groupOut ? child.In(*groupOut, o.Name.c_str(), type) : Conv("0.0", T::Float, type);
					Decl(n, o.Name, type, expr);
				}
				m_Transmission |= child.m_Transmission;
				m_PartialAlpha |= child.m_PartialAlpha;
				m_ShaderToRGB |= child.m_ShaderToRGB;
			}

			// Records a variable declared manually (multi-line nodes)
			void RegisterOut(const Node& n, const char* outName, T type) {
				if (const Pin* pin = n.FindOutput(outName)) {
					m_OutVars[pin->PinID] = Var(n, outName);
					m_OutTypes[pin->PinID] = type;
				}
			}

			void RegisterName(const Node& n, int slot) {
				const std::string name = Prop(n, "Name");
				if (!name.empty()) m_Out.NamedParams[name] = slot;
			}

			std::string LegacyOutput(const Node& n) {
				const std::string b = "legacy_out";
				std::string code = "LnxBSDF " + b + ";\n";
				code += "\t" + b + ".BaseColor = " + In(n, "Base Color", T::Vec3) + ";\n";
				code += "\t" + b + ".Metallic = clamp(" + In(n, "Metallic", T::Float) + ", 0.0, 1.0);\n";
				code += "\t" + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);\n";
				code += "\t" + b + ".Specular = clamp(" + In(n, "Specular", T::Float) + ", 0.0, 1.0);\n";
				code += "\t" + b + ".Alpha = 1.0;\n";
				const Pin* normal = n.FindInput("Normal");
				code += "\t" + b + ".Normal = " + ((normal && m_Graph.GetLinkInto(normal->PinID))
					? "lnx_normal_map(" + In(n, "Normal", T::Vec4) + ", " + In(n, "Normal Strength", T::Float) + ")"
					: std::string("LNX_NORMAL")) + ";\n";
				code += "\t" + b + ".AO = clamp(" + In(n, "Ambient Occlusion", T::Float) + ", 0.0, 1.0);\n";
				code += "\t" + b + ".Subsurface = vec4(" + In(n, "Subsurface Color", T::Vec3) + ", " + In(n, "Subsurface", T::Float) + ");\n";
				code += "\t" + b + ".Emission = " + In(n, "Emission", T::Vec3) + " * " + In(n, "Emission Strength", T::Float) + ";";
				Line(code);
				return b;
			}

			const NodeGraph& m_Graph;
			CompiledGraph& m_Out;
			std::string m_OwnBody;
			std::string& m_Body;
			const Bindings* m_GroupInputs = nullptr;
			int m_CurveCounter = 0;
		public:
			// What the surface needs from the renderer (set while generating)
			bool m_Transmission = false;   // transmission / glass / transparent closures
			bool m_PartialAlpha = false;   // alpha below 1 (or driven by a link)
			bool m_ShaderToRGB = false;    // needs real lighting inside the material shader
			std::unordered_map<ID, std::string> m_OutVars;
			std::unordered_map<ID, T> m_OutTypes;
			std::unordered_set<ID> m_Done, m_Visiting;
		};

		uint64_t HashString(const std::string& s) {
			uint64_t h = 14695981039346656037ull;
			for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
			return h;
		}
	}

	CompiledGraph Compile(const NodeGraph& graph) {
		CompiledGraph out;
		Generator gen(graph, out);
		const std::string surface = gen.Surface();

		// Render path: sorted transparent pass for transmission / blended alpha, forward for Shader to RGB
		std::string method = "Auto";
		if (const Node* o = graph.FindNodeByType(MaterialOutput)) {
			auto it = o->Properties.find("Render Method");
			if (it != o->Properties.end() && !it->second.empty()) method = it->second;
		}
		out.Blended = gen.m_Transmission || method == "Blended" || (method == "Auto" && gen.m_PartialAlpha);
		out.Forward = out.Blended || gen.m_ShaderToRGB;

		std::string code;
		code.reserve(32 * 1024);
		code += kLibCore;
		code += kLibImage;
		code += kLibNoise;
		code += kLibNodes;
		code += kLibNodes2;
		code += kLibMath;
		code += "\nLnxBSDF lnx_evaluate_graph() {\n";
		code += gen.Body();
		code += "\treturn " + surface + ";\n}\n";
		out.Code = std::move(code);
		out.CodeHash = HashString(out.Code);
		return out;
	}

	CompiledGraph CompilePreviews(const NodeGraph& graph, std::vector<ID>& order) {
		CompiledGraph out;
		Generator gen(graph, out);
		order.clear();
		std::string select;
		for (const auto& n : graph.GetNodes()) {
			if (n.Outputs.empty() || n.IsReroute() || n.TypeName == MaterialOutput || n.TypeName == OutputNode || n.TypeName == GroupOutputType) continue;
			const std::string expr = gen.PreviewExpr(n);
			select += "\tif (target == " + std::to_string(order.size()) + ") return " + expr + ";\n";
			order.push_back(n.NodeID);
		}
		std::string code;
		code.reserve(32 * 1024);
		code += kLibCore;
		code += kLibImage;
		code += kLibNoise;
		code += kLibNodes;
		code += kLibNodes2;
		code += kLibMath;
		code += "\nvec4 lnx_preview(int target) {\n";
		code += gen.Body();
		code += select;
		code += "\treturn vec4(0.0, 0.0, 0.0, 1.0);\n}\n";
		out.Code = std::move(code);
		out.CodeHash = HashString(out.Code);
		return out;
	}

	glm::vec4 EvaluateParam(const NodeGraph& graph, const ParamRef& ref) {
		const Node* node = graph.FindNodeDeep(ref.Node);
		if (!node) return glm::vec4(0.0f);
		if (!ref.Curve.empty()) {
			auto it = node->Properties.find(ref.Curve);
			const auto points = ParseCurve(it != node->Properties.end() ? it->second : "");
			const auto tangents = CurveTangents(points);
			if (ref.RampStop >= 0 && ref.RampStop < static_cast<int>(points.size()))
				return glm::vec4(points[ref.RampStop].X, points[ref.RampStop].Y, tangents[ref.RampStop], 0.0f);
			return glm::vec4(0.0f);
		}
		if (ref.RampStop >= 0) {
			auto it = node->Properties.find("Stops");
			const auto stops = ParseRamp(it != node->Properties.end() ? it->second : "");
			if (ref.RampStop < static_cast<int>(stops.size()))
				return ref.RampPosition ? glm::vec4(stops[ref.RampStop].Position) : stops[ref.RampStop].Color;
			return glm::vec4(0.0f);
		}
		if (ref.Pin == 0) return node->Value;
		if (const Pin* pin = graph.FindPinDeep(ref.Pin)) return pin->DefaultValue;
		return glm::vec4(0.0f);
	}

	uint32_t SlotForTexture(int index) {
		return index < 8 ? static_cast<uint32_t>(index) : static_cast<uint32_t>(12 + (index - 8));
	}

	// ============================================================================
	// SHADER SOURCE
	// ============================================================================

	std::string BuildShaderSource(const std::string& templatePath, const std::string& graphCode, const char* stageDefine, const char* extraDefine) {
		std::ifstream in(templatePath, std::ios::binary);
		if (!in) {
			LNX_LOG_ERROR("MaterialGraph: shader template not found: {0}", templatePath);
			return {};
		}
		std::stringstream ss;
		ss << in.rdbuf();
		std::string src = ss.str();

		const std::string marker = "// @LNX_GRAPH_CODE@";
		const size_t at = src.find(marker);
		if (at == std::string::npos) {
			LNX_LOG_ERROR("MaterialGraph: template {0} has no graph code marker", templatePath);
			return {};
		}
		src.replace(at, marker.size(), graphCode);

		const size_t version = src.find("#version");
		const size_t eol = version == std::string::npos ? 0 : src.find('\n', version) + 1;
		std::string defines = std::string("#define ") + stageDefine + "\n#define LNX_MATERIAL_GRAPH\n";
		if (extraDefine && *extraDefine) defines += std::string("#define ") + extraDefine + "\n";
		src.insert(eol, defines);
		return src;
	}

	// ============================================================================
	// RUNTIME
	// ============================================================================

	void GraphRuntime::Update(const NodeGraph& graph) {
		CompiledGraph compiled = Compile(graph);
		if (!compiled.Errors.empty()) {
			for (const auto& e : compiled.Errors) LNX_LOG_WARN("Material graph: {0}", e);
		}
		const bool codeChanged = compiled.CodeHash != m_Compiled.CodeHash;
		const bool texturesChanged = compiled.TexturePaths != m_Compiled.TexturePaths;
		m_Compiled = std::move(compiled);
		if (codeChanged) {
			for (int i = 0; i < 3; i++) { m_Shaders[i].reset(); m_Tried[i] = false; }
			m_ShaderFailed = false;
		}
		if (texturesChanged) m_Textures.clear();
		m_ParamsVersion = 0;   // force a params refresh
	}

	Ref<Shader> GraphRuntime::GetOrCreateShader(Mode mode) {
		const int m = static_cast<int>(mode);
		Ref<Shader>& shader = m_Shaders[m];
		if (shader || m_Tried[m]) return shader;
		m_Tried[m] = true;

		const char* tmpl = mode == Mode::Deferred ? "assets/shaders/Deferred_Geometry.glsl" : "assets/shaders/Mesh3D.glsl";
		const char* extra = mode == Mode::ForwardHDR ? "LNX_FORWARD_HDR" : nullptr;
		const std::string vs = BuildShaderSource(tmpl, m_Compiled.Code, "VERTEX", extra);
		const std::string fs = BuildShaderSource(tmpl, m_Compiled.Code, "FRAGMENT", extra);
		if (vs.empty() || fs.empty()) { m_ShaderFailed = true; return nullptr; }

		static const char* suffix[] = { "D", "F", "H" };
		char name[64];
		snprintf(name, sizeof(name), "MaterialGraph_%016llx_%s", static_cast<unsigned long long>(m_Compiled.CodeHash), suffix[m]);
		shader = Shader::Create(name, vs, fs);
		if (!shader || !shader->IsValid()) {
			LNX_LOG_ERROR("Material graph shader failed to compile ({0}); source saved to Cache/FailedMaterialShader.glsl", suffix[m]);
			std::error_code ec;
			std::filesystem::create_directories("Cache", ec);
			std::ofstream("Cache/FailedMaterialShader.glsl") << fs;
			shader.reset();
			m_ShaderFailed = true;
		}
		return shader;
	}

	std::chrono::steady_clock::time_point GraphClockStart() {
		static const auto s_Start = std::chrono::steady_clock::now();
		return s_Start;
	}

	void GraphRuntime::RefreshParams(const NodeGraph& graph) {
		// Refresh only when the graph changed
		if (m_ParamsVersion != graph.GetVersion() || m_Params.empty()) {
			m_Params.assign(kMaxGraphParams, glm::vec4(0.0f));
			for (size_t i = 0; i < m_Compiled.Params.size() && i + 1 < kMaxGraphParams; i++)
				m_Params[i + 1] = EvaluateParam(graph, m_Compiled.Params[i]);
			m_ParamsVersion = graph.GetVersion();
		}
	}

	void GraphRuntime::RefreshTextures() {
		if (m_Textures.size() == m_Compiled.TexturePaths.size()) return;
		static auto& s_Cache = *new std::unordered_map<std::string, Ref<Texture2D>>();
		m_Textures.clear();
		for (const auto& path : m_Compiled.TexturePaths) {
			auto& cached = s_Cache[path];
			if (!cached || !cached->IsLoaded()) cached = Texture2D::Create(path);
			m_Textures.push_back(cached && cached->IsLoaded() ? cached : nullptr);
		}
	}

	void GraphRuntime::GatherData(const NodeGraph& graph, const std::unordered_map<std::string, glm::vec4>* overrides,
		std::vector<glm::vec4>& outParams, std::vector<Ref<Texture2D>>& outTextures) {
		RefreshParams(graph);
		RefreshTextures();
		const size_t used = std::min<size_t>(m_Compiled.Params.size() + 1, kMaxGraphParams);
		outParams.assign(m_Params.begin(), m_Params.begin() + used);
		if (overrides) {
			for (const auto& [name, value] : *overrides) {
				auto it = m_Compiled.NamedParams.find(name);
				if (it != m_Compiled.NamedParams.end() && it->second < static_cast<int>(used)) outParams[it->second] = value;
			}
		}
		const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - GraphClockStart()).count();
		outParams[0] = glm::vec4(time, 0.0f, 0.0f, 0.0f);
		outTextures = m_Textures;
	}

	Ref<Shader> GraphRuntime::Bind(bool deferred, const NodeGraph& graph, const std::unordered_map<std::string, glm::vec4>* overrides) {
		return Bind(deferred ? Mode::Deferred : Mode::Forward, graph, overrides);
	}

	Ref<Shader> GraphRuntime::Bind(Mode mode, const NodeGraph& graph, const std::unordered_map<std::string, glm::vec4>* overrides) {
		if (!m_Compiled.IsValid()) return nullptr;
		Ref<Shader> shader = GetOrCreateShader(mode);
		if (!shader) return nullptr;

		RefreshParams(graph);
		static const auto s_Start = GraphClockStart();
		// GL-owning statics are leaked on purpose: destroying them after the context is gone crashes at exit
		static Ref<UniformBuffer>& s_ParamsUBO = *new Ref<UniformBuffer>(UniformBuffer::Create(sizeof(glm::vec4) * kMaxGraphParams, kGraphParamsBinding));

		const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - s_Start).count();
		if (overrides && !overrides->empty()) {
			std::vector<glm::vec4> params = m_Params;
			for (const auto& [name, value] : *overrides) {
				auto it = m_Compiled.NamedParams.find(name);
				if (it != m_Compiled.NamedParams.end() && it->second < kMaxGraphParams) params[it->second] = value;
			}
			params[0] = glm::vec4(time, 0.0f, 0.0f, 0.0f);
			s_ParamsUBO->SetData(params.data(), sizeof(glm::vec4) * kMaxGraphParams);
		}
		else {
			m_Params[0] = glm::vec4(time, 0.0f, 0.0f, 0.0f);
			s_ParamsUBO->SetData(m_Params.data(), sizeof(glm::vec4) * kMaxGraphParams);
		}

		shader->Bind();

		RefreshTextures();
		static Ref<Texture2D>& s_White = *new Ref<Texture2D>([] {
			auto t = Texture2D::Create(1, 1);
			uint32_t white = 0xFFFFFFFF;
			t->SetData(&white, sizeof(white));
			return t;
		}());
		for (size_t i = 0; i < m_Textures.size(); i++)
			(m_Textures[i] ? m_Textures[i] : s_White)->Bind(SlotForTexture(static_cast<int>(i)));

		return shader;
	}

} // namespace Lunex::MaterialGraph
