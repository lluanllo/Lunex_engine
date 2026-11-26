#include "stpch.h"
#include "MaterialGraphCompiler.h"

#include "MaterialGraph.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBuffer.h"
#include "Log/Log.h"

#include <chrono>
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
};

LnxBSDF lnx_bsdf_default() {
	LnxBSDF b;
	b.BaseColor = vec3(0.8); b.Metallic = 0.0; b.Roughness = 0.5; b.Specular = 0.5;
	b.Emission = vec3(0.0); b.Normal = LNX_NORMAL; b.AO = 1.0; b.Alpha = 1.0; b.Subsurface = vec4(0.0);
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
	return r;
}

LnxBSDF lnx_bsdf_add(LnxBSDF a, LnxBSDF b) {
	LnxBSDF r = lnx_bsdf_mix(a, b, 0.5);
	r.BaseColor = min(a.BaseColor + b.BaseColor, vec3(1.0));
	r.Emission = a.Emission + b.Emission;
	r.Alpha = max(a.Alpha, b.Alpha);
	return r;
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
			Generator(const NodeGraph& graph, CompiledGraph& out) : m_Graph(graph), m_Out(out) {}

			std::string Surface() {
				if (const Node* out = m_Graph.FindNodeByType(MaterialOutput))
					return In(*out, "Surface", T::BSDF);
				if (const Node* legacy = m_Graph.FindNodeByType(OutputNode))
					return LegacyOutput(*legacy);
				m_Out.Errors.push_back("The graph has no Material Output node");
				return "lnx_bsdf_default()";
			}

			const std::string& Body() const { return m_Body; }

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
				const NodeTypeInfo* info = m_Graph.GetRegistry() ? m_Graph.GetRegistry()->Get(n.TypeName) : nullptr;
				if (!info) return 0;
				const std::string value = Prop(n, key);
				for (const auto& e : info->Enums) {
					if (e.Key != key) continue;
					for (size_t i = 0; i < e.Options.size(); i++) if (e.Options[i] == value) return static_cast<int>(i);
				}
				return 0;
			}

			bool Toggle(const Node& n, const char* key) { return Prop(n, key, "0") == "1"; }

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

				if (const Link* link = m_Graph.GetLinkInto(pin->PinID)) {
					const Node* src = m_Graph.FindPinOwner(link->FromPin);
					const Pin* srcPin = m_Graph.FindPin(link->FromPin);
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
				const NodeTypeInfo* info = m_Graph.GetRegistry() ? m_Graph.GetRegistry()->Get(n.TypeName) : nullptr;
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

				// ---------------- Shaders ----------------
				if (t == PrincipledBSDF) {
					const std::string b = Var(n, "BSDF");
					std::string code = "LnxBSDF " + b + ";\n";
					code += "\t" + b + ".BaseColor = " + In(n, "Base Color", T::Vec3) + ";\n";
					code += "\t" + b + ".Metallic = clamp(" + In(n, "Metallic", T::Float) + ", 0.0, 1.0);\n";
					code += "\t" + b + ".Roughness = clamp(" + In(n, "Roughness", T::Float) + ", 0.0, 1.0);\n";
					code += "\t" + b + ".Specular = clamp(" + In(n, "Specular", T::Float) + ", 0.0, 1.0);\n";
					code += "\t" + b + ".Alpha = " + In(n, "Alpha", T::Float) + ";\n";
					code += "\t" + b + ".Normal = normalize(" + In(n, "Normal", T::Vec3, "LNX_NORMAL") + ");\n";
					code += "\t" + b + ".AO = clamp(" + In(n, "Ambient Occlusion", T::Float) + ", 0.0, 1.0);\n";
					code += "\t" + b + ".Subsurface = vec4(" + In(n, "Subsurface Color", T::Vec3) + ", clamp(" + In(n, "Subsurface Weight", T::Float) + ", 0.0, 1.0));\n";
					code += "\t" + b + ".Emission = " + In(n, "Emission Color", T::Vec3) + " * " + In(n, "Emission Strength", T::Float) + ";";
					Line(code);
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

				// ---------------- Input ----------------
				if (t == TexCoordNode) {
					Decl(n, "UV", T::Vec3, "vec3(LNX_UV, 0.0)");
					Decl(n, "Generated", T::Vec3, "LNX_POS");
					Decl(n, "Normal", T::Vec3, "LNX_NORMAL");
					Decl(n, "Reflection", T::Vec3, "reflect(-lnx_view_dir(), LNX_NORMAL)");
					return;
				}
				if (t == GeometryNode) {
					Decl(n, "Position", T::Vec3, "LNX_POS");
					Decl(n, "Normal", T::Vec3, "LNX_NORMAL");
					Decl(n, "Tangent", T::Vec3, "normalize(LNX_TBN[0])");
					Decl(n, "Incoming", T::Vec3, "lnx_view_dir()");
					return;
				}
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
							sample = Prop(n, "Interpolation", "Linear") == "Closest"
								? "texelFetch(" + s + ", ivec2(fract((" + uv + ").xy) * vec2(textureSize(" + s + ", 0))), 0)"
								: "texture(" + s + ", (" + uv + ").xy)";
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
				if (t == NoiseTexture) {
					std::string v = In(n, "Vector", T::Vec3, "LNX_POS");
					if (Prop(n, "Dimensions", "3D") == "2D") v = "vec3((" + v + ").xy, 0.0)";
					Line("vec4 " + id + "_n = lnx_noise_tex(" + v + ", " + In(n, "Scale", T::Float) + ", " + In(n, "Detail", T::Float) + ", " +
						In(n, "Roughness", T::Float) + ", " + In(n, "Distortion", T::Float) + ");");
					Decl(n, "Fac", T::Float, id + "_n.w");
					Decl(n, "Color", T::Vec4, "vec4(" + id + "_n.rgb, 1.0)");
					return;
				}
				if (t == VoronoiTexture) {
					Line("float " + id + "_d; vec3 " + id + "_c; vec3 " + id + "_p;");
					Line("lnx_voronoi(" + In(n, "Vector", T::Vec3, "LNX_POS") + ", " + In(n, "Scale", T::Float) + ", " + In(n, "Randomness", T::Float) + ", " +
						std::to_string(Op(n, "Feature")) + ", " + id + "_d, " + id + "_c, " + id + "_p);");
					Decl(n, "Distance", T::Float, id + "_d");
					Decl(n, "Color", T::Vec4, "vec4(" + id + "_c, 1.0)");
					Decl(n, "Position", T::Vec3, id + "_p");
					return;
				}
				if (t == WaveTexture) {
					Line("float " + id + "_w = lnx_wave(" + In(n, "Vector", T::Vec3, "LNX_POS") + ", " + In(n, "Scale", T::Float) + ", " + In(n, "Distortion", T::Float) + ", " +
						In(n, "Detail", T::Float) + ", " + In(n, "Phase Offset", T::Float) + ", " + std::to_string(Op(n, "Type")) + ", " + std::to_string(Op(n, "Profile")) + ");");
					Decl(n, "Color", T::Vec4, "vec4(vec3(" + id + "_w), 1.0)");
					Decl(n, "Fac", T::Float, id + "_w");
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
				if (t == GradientTexture) {
					Line("float " + id + "_g = lnx_gradient(" + In(n, "Vector", T::Vec3, "vec3(LNX_UV, 0.0)") + ", " + std::to_string(Op(n, "Type")) + ");");
					Decl(n, "Color", T::Vec4, "vec4(vec3(" + id + "_g), 1.0)");
					Decl(n, "Fac", T::Float, id + "_g");
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

				// ---------------- Legacy ----------------
				if (t == MultiplyNode) {
					Decl(n, "Result", T::Vec4, In(n, "A", T::Vec4) + " * " + In(n, "B", T::Vec4));
					return;
				}

				m_Out.Errors.push_back("Node type not supported by the shader compiler: " + n.Title);
				for (const auto& out : n.Outputs) Decl(n, out.Name, TypeOf(out.Type), Conv("0.0", T::Float, TypeOf(out.Type)));
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
			std::string m_Body;
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

		std::string code;
		code.reserve(32 * 1024);
		code += kLibCore;
		code += kLibNoise;
		code += kLibMath;
		code += "\nLnxBSDF lnx_evaluate_graph() {\n";
		code += gen.Body();
		code += "\treturn " + surface + ";\n}\n";
		out.Code = std::move(code);
		out.CodeHash = HashString(out.Code);
		return out;
	}

	uint32_t SlotForTexture(int index) {
		return index < 8 ? static_cast<uint32_t>(index) : static_cast<uint32_t>(12 + (index - 8));
	}

	// ============================================================================
	// SHADER SOURCE
	// ============================================================================

	std::string BuildShaderSource(const std::string& templatePath, const std::string& graphCode, const char* stageDefine) {
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
		src.insert(eol, std::string("#define ") + stageDefine + "\n#define LNX_MATERIAL_GRAPH\n");
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
			m_Deferred.reset(); m_Forward.reset();
			m_DeferredTried = m_ForwardTried = false;
			m_ShaderFailed = false;
		}
		if (texturesChanged) m_Textures.clear();
		m_ParamsVersion = 0;   // force a params refresh
	}

	Ref<Shader> GraphRuntime::GetOrCreateShader(bool deferred) {
		Ref<Shader>& shader = deferred ? m_Deferred : m_Forward;
		bool& tried = deferred ? m_DeferredTried : m_ForwardTried;
		if (shader || tried) return shader;
		tried = true;

		const char* tmpl = deferred ? "assets/shaders/Deferred_Geometry.glsl" : "assets/shaders/Mesh3D.glsl";
		const std::string vs = BuildShaderSource(tmpl, m_Compiled.Code, "VERTEX");
		const std::string fs = BuildShaderSource(tmpl, m_Compiled.Code, "FRAGMENT");
		if (vs.empty() || fs.empty()) { m_ShaderFailed = true; return nullptr; }

		char name[64];
		snprintf(name, sizeof(name), "MaterialGraph_%016llx_%s", static_cast<unsigned long long>(m_Compiled.CodeHash), deferred ? "D" : "F");
		shader = Shader::Create(name, vs, fs);
		if (!shader || !shader->IsValid()) {
			LNX_LOG_ERROR("Material graph shader failed to compile ({0})", deferred ? "deferred" : "forward");
			shader.reset();
			m_ShaderFailed = true;
		}
		return shader;
	}

	Ref<Shader> GraphRuntime::Bind(bool deferred, const NodeGraph& graph, const std::unordered_map<std::string, glm::vec4>* overrides) {
		if (!m_Compiled.IsValid()) return nullptr;
		Ref<Shader> shader = GetOrCreateShader(deferred);
		if (!shader) return nullptr;

		// ---- Params (refresh only when the graph changed) ----
		if (m_ParamsVersion != graph.GetVersion() || m_Params.empty()) {
			m_Params.assign(kMaxGraphParams, glm::vec4(0.0f));
			for (size_t i = 0; i < m_Compiled.Params.size() && i + 1 < kMaxGraphParams; i++) {
				const ParamRef& ref = m_Compiled.Params[i];
				const Node* node = graph.FindNode(ref.Node);
				if (!node) continue;
				glm::vec4 v(0.0f);
				if (ref.RampStop >= 0) {
					auto it = node->Properties.find("Stops");
					const auto stops = ParseRamp(it != node->Properties.end() ? it->second : "");
					if (ref.RampStop < static_cast<int>(stops.size()))
						v = ref.RampPosition ? glm::vec4(stops[ref.RampStop].Position) : stops[ref.RampStop].Color;
				}
				else if (ref.Pin == 0) {
					v = node->Value;
				}
				else if (const Pin* pin = graph.FindPin(ref.Pin)) {
					v = pin->DefaultValue;
				}
				m_Params[i + 1] = v;
			}
			m_ParamsVersion = graph.GetVersion();
		}

		static const auto s_Start = std::chrono::steady_clock::now();
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

		// ---- Textures ----
		if (m_Textures.size() != m_Compiled.TexturePaths.size()) {
			static auto& s_Cache = *new std::unordered_map<std::string, Ref<Texture2D>>();
			m_Textures.clear();
			for (const auto& path : m_Compiled.TexturePaths) {
				auto& cached = s_Cache[path];
				if (!cached || !cached->IsLoaded()) cached = Texture2D::Create(path);
				m_Textures.push_back(cached && cached->IsLoaded() ? cached : nullptr);
			}
		}
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
