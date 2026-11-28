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
#include "Renderer/EnvironmentMap.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Texture.h"
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
		// SHADERS (GLSL 450, compiled to SPIR-V with shaderc at startup)
		// ============================================================================

		const char* kCommon = R"GLSL(
#version 450
layout(set = 0, binding = 0) uniform FrameData {
	mat4 View; mat4 Proj; mat4 ViewProj; mat4 InvViewProj;
	vec4 CamPos;
	vec4 EnvParams;    // x intensity, y rotation, z has env, w max mip
	ivec4 Counts;      // x lights
} F;
struct Light { vec4 PosRange; vec4 DirType; vec4 Color; vec4 Cone; };
layout(set = 0, binding = 1, std430) readonly buffer Lights { Light L[]; };
layout(set = 0, binding = 2, std430) readonly buffer Bones { mat4 B[]; };
struct Material { vec4 Albedo; vec4 Emission; vec4 Params; ivec4 Maps; ivec4 Maps2; vec4 UV; vec4 Subsurface; };
layout(set = 0, binding = 3, std430) readonly buffer Materials { Material M[]; };
layout(set = 0, binding = 4) uniform sampler2D uEnv;
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

		const char* kMeshFS = R"GLSL(
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;
layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntity;
layout(set = 1, binding = 0) uniform sampler2D tAlbedo;
layout(set = 1, binding = 1) uniform sampler2D tNormal;
layout(set = 1, binding = 2) uniform sampler2D tMetallic;
layout(set = 1, binding = 3) uniform sampler2D tRoughness;
layout(set = 1, binding = 4) uniform sampler2D tAO;
layout(set = 1, binding = 5) uniform sampler2D tEmission;
layout(set = 1, binding = 6) uniform sampler2D tLayered;

vec3 ACES(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
float D_GGX(float NdotH, float r) { float a = r * r; float a2 = a * a; float d = NdotH * NdotH * (a2 - 1.0) + 1.0; return a2 / (PI * d * d); }
float V_Smith(float NdotV, float NdotL, float r) { float a = r * r; float gv = NdotL * sqrt(NdotV * NdotV * (1.0 - a * a) + a * a); float gl = NdotV * sqrt(NdotL * NdotL * (1.0 - a * a) + a * a); return 0.5 / max(gv + gl, 1e-5); }
vec3 F_Schlick(float c, vec3 f0) { return f0 + (1.0 - f0) * pow(1.0 - c, 5.0); }
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

	vec3 V = normalize(F.CamPos.xyz - vPos);
	float NdotV = max(dot(N, V), 1e-4);
	vec3 F0 = mix(vec3(0.08 * m.Params.w), albedo.rgb, metallic);
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < F.Counts.x; i++) {
		Light l = L[i];
		int type = int(l.DirType.w);
		vec3 Ld;
		float att = 1.0;
		if (type == 0) Ld = normalize(-l.DirType.xyz);
		else {
			vec3 d = l.PosRange.xyz - vPos;
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
		float NdotL = max(dot(N, Ld), 0.0);
		if (NdotL <= 0.0 || att <= 0.0) continue;
		vec3 H = normalize(V + Ld);
		vec3 Fs = F_Schlick(max(dot(V, H), 0.0), F0);
		vec3 spec = D_GGX(max(dot(N, H), 0.0), roughness) * V_Smith(NdotV, NdotL, roughness) * Fs;
		vec3 kd = (1.0 - Fs) * (1.0 - metallic);
		Lo += (kd * albedo.rgb / PI + spec) * l.Color.rgb * l.Color.w * NdotL * att;
	}
	// Image based lighting from the HDRI (mip by roughness)
	vec3 Fa = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
	vec3 diffuseIBL = Env(N, F.EnvParams.w - 1.0) * albedo.rgb * (1.0 - Fa) * (1.0 - metallic);
	vec3 specIBL = Env(reflect(-V, N), roughness * F.EnvParams.w) * Fa;
	vec3 color = Lo + (diffuseIBL + specIBL) * ao;
	vec3 emission = m.Emission.rgb;
	if (m.Maps2.y != 0) emission *= pow(texture(tEmission, uv).rgb, vec3(2.2));
	color += emission;
	oColor = vec4(pow(ACES(color), vec3(1.0 / 2.2)), 1.0);
	oEntity = P.Entity;
}
)GLSL";

		// Fullscreen triangle: sky (far plane) and grid (y = 0 plane, depth tested)
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
vec3 ACES(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
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
	oColor = vec4(pow(ACES(c), vec3(1.0 / 2.2)), 1.0);
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
	gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;
}
)GLSL";

		std::vector<uint32_t> Compile(const std::string& source, shaderc_shader_kind kind, const char* name, bool skinned = false) {
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
			if (skinned) options.AddMacroDefinition("SKINNED");
			auto result = compiler.CompileGlslToSpv(source, kind, name, options);
			if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
				LNX_LOG_ERROR("Vulkan scene shader {0}: {1}", name, result.GetErrorMessage());
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
		};
		struct GPULight { glm::vec4 PosRange, DirType, Color, Cone; };
		struct GPUMaterial {
			glm::vec4 Albedo;
			glm::vec4 Emission;    // rgb emission, w roughness multiplier
			glm::vec4 Params;      // metallic, roughness, normal strength, specular
			glm::ivec4 Maps;       // albedo, normal, metallic, roughness
			glm::ivec4 Maps2;      // ao, emission, layered, layered flags | channels
			glm::vec4 UV;          // tiling xy, offset zw
			glm::vec4 Subsurface;  // w = metallic multiplier
		};
		struct PushData { glm::mat4 Model; int Material; int Entity; int BoneOffset; int Skinned; };

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

		struct FrameResources {
			DynamicBuffer Frame, Lights, Bones, Materials;
			VkDescriptorPool Pool = VK_NULL_HANDLE;
			VkDescriptorSet Set0 = VK_NULL_HANDLE;
			Buffer Readback;
			bool ReadbackPending = false;
		};

		struct Renderer {
			bool Initialized = false;
			bool Failed = false;
			VkDescriptorSetLayout Set0Layout = VK_NULL_HANDLE, Set1Layout = VK_NULL_HANDLE;
			VkPipelineLayout Layout = VK_NULL_HANDLE;
			VkPipeline MeshPipe = VK_NULL_HANDLE, SkinnedPipe = VK_NULL_HANDLE, SkyPipe = VK_NULL_HANDLE, GridPipe = VK_NULL_HANDLE;
			VkDescriptorPool MaterialPool = VK_NULL_HANDLE;
			std::unordered_map<size_t, VkDescriptorSet> MaterialSets;
			FrameResources Frames[kFramesInFlight];

			Ref<VulkanTexture2D> Color, Entity, Depth;
			uint32_t Width = 0, Height = 0;

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

		enum class PipeKind { Mesh, Sky, Grid };

		VkPipeline CreatePipeline(VkShaderModule vs, VkShaderModule fs, PipeKind kind, bool skinned) {
			auto& ctx = VulkanContext::Get();
			VkPipelineShaderStageCreateInfo stages[2]{};
			stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main" };
			stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main" };

			VkVertexInputBindingDescription bindings[2] = {
				{ 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX },
				{ 1, sizeof(glm::ivec4) + sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX },
			};
			VkVertexInputAttributeDescription attrs[7] = {
				{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position) },
				{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal) },
				{ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, TexCoords) },
				{ 3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Tangent) },
				{ 4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Bitangent) },
				{ 6, 1, VK_FORMAT_R32G32B32A32_SINT, 0 },
				{ 7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::ivec4) },
			};
			VkPipelineVertexInputStateCreateInfo vertex{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
			if (kind == PipeKind::Mesh) {
				vertex.vertexBindingDescriptionCount = skinned ? 2 : 1;
				vertex.pVertexBindingDescriptions = bindings;
				vertex.vertexAttributeDescriptionCount = skinned ? 7 : 5;
				vertex.pVertexAttributeDescriptions = attrs;
			}
			VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
			ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			// GL clip space: z in [-1, 1] (glm projections) without touching the matrices
			VkPipelineViewportDepthClipControlCreateInfoEXT clip{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
			clip.negativeOneToOne = VK_TRUE;
			VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
			vp.viewportCount = 1;
			vp.scissorCount = 1;
			if (ctx.HasDepthClipControl() && kind == PipeKind::Mesh) vp.pNext = &clip;

			VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
			rs.polygonMode = VK_POLYGON_MODE_FILL;
			rs.cullMode = VK_CULL_MODE_NONE;     // double sided like the deferred path
			rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rs.lineWidth = 1.0f;
			VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
			ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
			ds.depthTestEnable = VK_TRUE;
			ds.depthWriteEnable = kind == PipeKind::Mesh ? VK_TRUE : VK_FALSE;
			ds.depthCompareOp = kind == PipeKind::Sky ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;

			VkPipelineColorBlendAttachmentState blend[2]{};
			blend[0].colorWriteMask = 0xF;
			blend[1].colorWriteMask = kind == PipeKind::Grid ? 0 : VK_COLOR_COMPONENT_R_BIT;
			if (kind == PipeKind::Grid) {
				blend[0].blendEnable = VK_TRUE;
				blend[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				blend[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				blend[0].colorBlendOp = VK_BLEND_OP_ADD;
				blend[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				blend[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				blend[0].alphaBlendOp = VK_BLEND_OP_ADD;
			}
			VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
			cb.attachmentCount = 2;
			cb.pAttachments = blend;
			const VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo dynamic{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
			dynamic.dynamicStateCount = 2;
			dynamic.pDynamicStates = dyn;

			const VkFormat colors[2] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32_SINT };
			VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			rendering.colorAttachmentCount = 2;
			rendering.pColorAttachmentFormats = colors;
			rendering.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

			VkGraphicsPipelineCreateInfo info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
			info.pNext = &rendering;
			info.stageCount = 2;
			info.pStages = stages;
			info.pVertexInputState = &vertex;
			info.pInputAssemblyState = &ia;
			info.pViewportState = &vp;
			info.pRasterizationState = &rs;
			info.pMultisampleState = &ms;
			info.pDepthStencilState = &ds;
			info.pColorBlendState = &cb;
			info.pDynamicState = &dynamic;
			info.layout = R().Layout;
			VkPipeline pipeline = VK_NULL_HANDLE;
			if (vkCreateGraphicsPipelines(ctx.GetDevice(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
				LNX_LOG_ERROR("Vulkan scene renderer: pipeline creation failed");
			return pipeline;
		}

		Ref<VulkanTexture2D> SolidTexture(uint32_t rgba) {
			RHI::TextureDesc desc(1, 1, RHI::TextureFormat::RGBA8);
			return CreateRef<VulkanTexture2D>(desc, &rgba, sizeof(rgba));
		}

		bool Init() {
			auto& r = R();
			auto& ctx = VulkanContext::Get();
			const VkDevice dev = ctx.GetDevice();

			// Set 0: frame UBO, lights, bones, materials, environment
			VkDescriptorSetLayoutBinding b0[5] = {
				{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
				{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			};
			VkDescriptorSetLayoutCreateInfo l0{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			l0.bindingCount = 5;
			l0.pBindings = b0;
			vkCreateDescriptorSetLayout(dev, &l0, nullptr, &r.Set0Layout);
			// Set 1: material textures
			VkDescriptorSetLayoutBinding b1[7];
			for (uint32_t i = 0; i < 7; i++) b1[i] = { i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
			VkDescriptorSetLayoutCreateInfo l1{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			l1.bindingCount = 7;
			l1.pBindings = b1;
			vkCreateDescriptorSetLayout(dev, &l1, nullptr, &r.Set1Layout);

			VkPushConstantRange push{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData) };
			const VkDescriptorSetLayout sets[2] = { r.Set0Layout, r.Set1Layout };
			VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
			pl.setLayoutCount = 2;
			pl.pSetLayouts = sets;
			pl.pushConstantRangeCount = 1;
			pl.pPushConstantRanges = &push;
			vkCreatePipelineLayout(dev, &pl, nullptr, &r.Layout);

			const std::string common = kCommon;
			VkShaderModule meshVS = Module(Compile(common + kMeshVS, shaderc_vertex_shader, "mesh.vert"));
			VkShaderModule meshSkinVS = Module(Compile(common + kMeshVS, shaderc_vertex_shader, "mesh_skinned.vert", true));
			VkShaderModule meshFS = Module(Compile(common + kMeshFS, shaderc_fragment_shader, "mesh.frag"));
			VkShaderModule fullVS = Module(Compile(common + kFullscreenVS, shaderc_vertex_shader, "fullscreen.vert"));
			VkShaderModule skyFS = Module(Compile(common + kSkyFS, shaderc_fragment_shader, "sky.frag"));
			VkShaderModule gridFS = Module(Compile(common + kGridFS, shaderc_fragment_shader, "grid.frag"));
			if (!meshVS || !meshSkinVS || !meshFS || !fullVS || !skyFS || !gridFS) return false;
			r.MeshPipe = CreatePipeline(meshVS, meshFS, PipeKind::Mesh, false);
			r.SkinnedPipe = CreatePipeline(meshSkinVS, meshFS, PipeKind::Mesh, true);
			r.SkyPipe = CreatePipeline(fullVS, skyFS, PipeKind::Sky, false);
			r.GridPipe = CreatePipeline(fullVS, gridFS, PipeKind::Grid, false);
			for (VkShaderModule m : { meshVS, meshSkinVS, meshFS, fullVS, skyFS, gridFS }) vkDestroyShaderModule(dev, m, nullptr);
			if (!r.MeshPipe || !r.SkinnedPipe || !r.SkyPipe || !r.GridPipe) return false;

			const VkDescriptorPoolSize sizes[] = {
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 } };
			for (auto& f : r.Frames) {
				VkDescriptorPoolCreateInfo pool{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
				pool.maxSets = 4;
				pool.poolSizeCount = 3;
				pool.pPoolSizes = sizes;
				vkCreateDescriptorPool(dev, &pool, nullptr, &f.Pool);
				f.Readback = CreateBuffer(16, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			}
			const VkDescriptorPoolSize matSizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 * 7 } };
			VkDescriptorPoolCreateInfo matPool{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			matPool.maxSets = 4096;
			matPool.poolSizeCount = 1;
			matPool.pPoolSizes = matSizes;
			vkCreateDescriptorPool(dev, &matPool, nullptr, &r.MaterialPool);

			r.White = SolidTexture(0xFFFFFFFF);
			r.FlatNormal = SolidTexture(0xFFFF8080);
			r.Black = SolidTexture(0xFF000000);
			LNX_LOG_INFO("Vulkan scene renderer ready");
			return true;
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

		void EnsureTargets(uint32_t w, uint32_t h) {
			auto& r = R();
			if (r.Color && r.Width == w && r.Height == h) return;
			r.Width = w;
			r.Height = h;
			if (r.Color) { r.Color->Resize(w, h); r.Entity->Resize(w, h); r.Depth->Resize(w, h); return; }
			RHI::TextureDesc color = RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::RGBA8);
			RHI::TextureDesc entity = RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::R32I);
			RHI::TextureDesc depth = RHI::TextureDesc::RenderTarget(w, h, RHI::TextureFormat::Depth32F);
			r.Color = CreateRef<VulkanTexture2D>(color);
			r.Entity = CreateRef<VulkanTexture2D>(entity);
			r.Depth = CreateRef<VulkanTexture2D>(depth);
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

		// Descriptor set for a texture combination (cached)
		VkDescriptorSet MaterialSet(VulkanTexture2D* const tex[7]) {
			auto& r = R();
			size_t key = 1469598103934665603ull;
			for (int i = 0; i < 7; i++) key = (key ^ reinterpret_cast<size_t>(tex[i] ? tex[i]->GetView() : VK_NULL_HANDLE)) * 1099511628211ull;
			auto it = r.MaterialSets.find(key);
			if (it != r.MaterialSets.end()) return it->second;
			auto& ctx = VulkanContext::Get();
			VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			alloc.descriptorPool = r.MaterialPool;
			alloc.descriptorSetCount = 1;
			alloc.pSetLayouts = &r.Set1Layout;
			VkDescriptorSet set = VK_NULL_HANDLE;
			if (vkAllocateDescriptorSets(ctx.GetDevice(), &alloc, &set) != VK_SUCCESS) {
				// Pool full (textures reloaded many times): start over
				vkResetDescriptorPool(ctx.GetDevice(), r.MaterialPool, 0);
				r.MaterialSets.clear();
				vkAllocateDescriptorSets(ctx.GetDevice(), &alloc, &set);
			}
			VkDescriptorImageInfo images[7];
			VkWriteDescriptorSet writes[7];
			for (uint32_t i = 0; i < 7; i++) {
				VulkanTexture2D* t = tex[i] ? tex[i] : (i == 1 ? r.FlatNormal.get() : r.White.get());
				images[i] = { t->GetDefaultSampler(), t->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
				writes[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, i, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &images[i], nullptr, nullptr };
			}
			vkUpdateDescriptorSets(ctx.GetDevice(), 7, writes, 0, nullptr);
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
		};

		// Fills a GPU material from the engine's material data; returns the texture set
		template<typename UniformData>
		VkDescriptorSet FillMaterial(GPUMaterial& out, const UniformData& u, const Ref<MaterialAsset>& asset) {
			out.Albedo = u.Albedo;
			out.Emission = glm::vec4(u.EmissionColor * u.EmissionIntensity, u.RoughnessMultiplier);
			out.Params = glm::vec4(u.Metallic, u.Roughness, u.NormalIntensity, u.Specular);
			out.UV = (u.UVTilingOffset.x == 0.0f && u.UVTilingOffset.y == 0.0f) ? glm::vec4(1, 1, 0, 0) : u.UVTilingOffset;
			out.Subsurface = glm::vec4(glm::vec3(u.SubsurfaceColorAmount), u.MetallicMultiplier);
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
			return MaterialSet(tex);
		}

	}

	// ============================================================================
	// PUBLIC
	// ============================================================================

	VulkanSceneRenderer& VulkanSceneRenderer::Get() {
		static VulkanSceneRenderer& s = *new VulkanSceneRenderer();
		return s;
	}

	uint32_t VulkanSceneRenderer::GetOutputTextureID() const {
		auto& r = R();
		return r.Color ? r.Color->GetRendererID() : 0;
	}

	void VulkanSceneRenderer::Render(Scene& scene, const VulkanRenderView& view) {
		auto& r = R();
		auto& ctx = VulkanContext::Get();
		if (!ctx.IsFrameActive() || r.Failed || view.Width == 0 || view.Height == 0) return;
		if (!r.Initialized) {
			r.Initialized = true;
			if (!Init()) { r.Failed = true; LNX_LOG_ERROR("Vulkan scene renderer could not start"); return; }
		}
		const auto t0 = std::chrono::high_resolution_clock::now();
		r.FrameCounter++;
		m_Stats = {};
		const VkDevice dev = ctx.GetDevice();
		FrameResources& fr = r.Frames[ctx.GetFrameIndex()];

		// Picking result of the previous use of this frame slot (its fence was waited in BeginFrame)
		if (fr.ReadbackPending) {
			std::memcpy(&m_PickedEntity, fr.Readback.Memory.Mapped, sizeof(int));
			fr.ReadbackPending = false;
		}

		EnsureTargets(view.Width, view.Height);
		UpdateEnvironment();

		// ---- Gather lights ----
		std::vector<GPULight> lights;
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
				lights.push_back(l);
				if (lights.size() >= 1024) break;
			}
		}
		m_Stats.Lights = static_cast<uint32_t>(lights.size());

		// ---- Gather draws (frustum culled) ----
		const glm::mat4 viewProj = view.Projection * view.View;
		const ViewFrustum frustum = ViewFrustum::FromViewProjection(viewProj);
		std::vector<GPUMaterial> materials;
		std::vector<glm::mat4> bones;
		std::vector<DrawItem> draws;
		materials.push_back({ glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), glm::vec4(0, 0, 0, 1), glm::vec4(0.0f, 0.5f, 1.0f, 0.5f), {}, {}, glm::vec4(1, 1, 0, 0), glm::vec4(0, 0, 0, 1) });
		VulkanTexture2D* none[7] = {};
		const VkDescriptorSet defaultSet = MaterialSet(none);

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
			if (matComp && matComp->Instance && !perSubmesh) {
				GPUMaterial gm{};
				entitySet = FillMaterial(gm, matComp->Instance->GetUniformData(), matComp->Instance->GetBaseAsset());
				entityMaterial = static_cast<int>(materials.size());
				materials.push_back(gm);
			}

			const auto& meshes = mc.MeshModel->GetMeshes();
			for (size_t i = 0; i < meshes.size(); i++) {
				const Ref<Mesh>& mesh = meshes[i];
				if (!mesh->IsSkinned()) {
					glm::vec3 mn, mx;
					Mesh::TransformAABB(world, mesh->GetAABBMin(), mesh->GetAABBMax(), mn, mx);
					if (!frustum.IntersectsAABB(mn, mx)) { m_Stats.Culled++; continue; }
				}
				const GPUMesh* g = GetMesh(*mesh);
				if (!g) continue;
				DrawItem d;
				d.Mesh = g;
				d.Model = world;
				d.Entity = static_cast<int>(e);
				d.Skinned = hasBones && mesh->IsSkinned() && g->Bones.Handle;
				d.BoneOffset = boneOffset;
				d.Material = entityMaterial;
				d.Textures = entitySet;
				if (perSubmesh) {
					if (Ref<MaterialAsset> sub = mc.GetSubmeshMaterial(static_cast<uint32_t>(i))) {
						GPUMaterial gm{};
						d.Textures = FillMaterial(gm, sub->GetUniformData(), sub);
						d.Material = static_cast<int>(materials.size());
						materials.push_back(gm);
					}
				}
				draws.push_back(d);
			}
		}
		// Fewer state changes: group by pipeline, then by texture set
		std::sort(draws.begin(), draws.end(), [](const DrawItem& a, const DrawItem& b) {
			if (a.Skinned != b.Skinned) return a.Skinned < b.Skinned;
			return a.Textures < b.Textures;
		});

		// ---- Upload frame data ----
		FrameUBO frame{};
		frame.View = view.View;
		frame.Proj = view.Projection;
		frame.ViewProj = viewProj;
		frame.InvViewProj = glm::inverse(viewProj);
		frame.CamPos = glm::vec4(view.CameraPosition, 1.0f);
		Ref<EnvironmentMap> envMap = SkyboxRenderer::GetGlobalEnvironment();
		frame.EnvParams = glm::vec4(envMap ? envMap->GetIntensity() : 1.0f, envMap ? glm::radians(envMap->GetRotation()) : 0.0f, r.Env ? 1.0f : 0.0f, r.EnvMips);
		frame.Counts = glm::ivec4(static_cast<int>(lights.size()), 0, 0, 0);
		if (bones.empty()) bones.push_back(glm::mat4(1.0f));
		if (lights.empty()) lights.push_back({});

		fr.Frame.Ensure(sizeof(FrameUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		fr.Lights.Ensure(lights.size() * sizeof(GPULight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		fr.Bones.Ensure(bones.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		fr.Materials.Ensure(materials.size() * sizeof(GPUMaterial), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		std::memcpy(fr.Frame.Buf.Memory.Mapped, &frame, sizeof(frame));
		std::memcpy(fr.Lights.Buf.Memory.Mapped, lights.data(), lights.size() * sizeof(GPULight));
		std::memcpy(fr.Bones.Buf.Memory.Mapped, bones.data(), bones.size() * sizeof(glm::mat4));
		std::memcpy(fr.Materials.Buf.Memory.Mapped, materials.data(), materials.size() * sizeof(GPUMaterial));

		// Set 0 for this frame (buffers may have been reallocated)
		vkResetDescriptorPool(dev, fr.Pool, 0);
		VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc.descriptorPool = fr.Pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &r.Set0Layout;
		vkAllocateDescriptorSets(dev, &alloc, &fr.Set0);
		VkDescriptorBufferInfo bufs[4] = {
			{ fr.Frame.Buf.Handle, 0, sizeof(FrameUBO) },
			{ fr.Lights.Buf.Handle, 0, VK_WHOLE_SIZE },
			{ fr.Bones.Buf.Handle, 0, VK_WHOLE_SIZE },
			{ fr.Materials.Buf.Handle, 0, VK_WHOLE_SIZE },
		};
		VulkanTexture2D* envTex = r.Env ? r.Env.get() : r.Black.get();
		VkDescriptorImageInfo envInfo{ VulkanContext::Get().GetSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT),
			envTex->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet writes[5] = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufs[0], nullptr },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufs[1], nullptr },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufs[2], nullptr },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufs[3], nullptr },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, fr.Set0, 4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &envInfo, nullptr, nullptr },
		};
		vkUpdateDescriptorSets(dev, 5, writes, 0, nullptr);

		// ---- Record ----
		VkCommandBuffer cmd = ctx.GetCommandBuffer();
		r.Color->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		r.Entity->Transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		r.Depth->Transition(cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

		VkRenderingAttachmentInfo colors[2]{};
		colors[0] = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		colors[0].imageView = r.Color->GetView();
		colors[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colors[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colors[0].clearValue.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
		colors[1] = colors[0];
		colors[1].imageView = r.Entity->GetView();
		colors[1].clearValue.color.int32[0] = -1;
		VkRenderingAttachmentInfo depth{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		depth.imageView = r.Depth->GetView();
		depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth.clearValue.depthStencil = { 1.0f, 0 };
		VkRenderingInfo rendering{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		rendering.renderArea = { { 0, 0 }, { view.Width, view.Height } };
		rendering.layerCount = 1;
		rendering.colorAttachmentCount = 2;
		rendering.pColorAttachments = colors;
		rendering.pDepthAttachment = &depth;
		vkCmdBeginRendering(cmd, &rendering);

		// Positive height: NDC -1 -> row 0, so image rows (and triangle winding) match the GL framebuffer
		VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(view.Width), static_cast<float>(view.Height), 0.0f, 1.0f };
		VkRect2D scissor{ { 0, 0 }, { view.Width, view.Height } };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 0, 1, &fr.Set0, 0, nullptr);

		VkPipeline bound = VK_NULL_HANDLE;
		VkDescriptorSet boundSet = VK_NULL_HANDLE;
		for (const DrawItem& d : draws) {
			const VkPipeline pipeline = d.Skinned ? r.SkinnedPipe : r.MeshPipe;
			if (pipeline != bound) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); bound = pipeline; }
			if (d.Textures != boundSet) { vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.Layout, 1, 1, &d.Textures, 0, nullptr); boundSet = d.Textures; }
			PushData push{ d.Model, d.Material, d.Entity, d.BoneOffset, d.Skinned ? 1 : 0 };
			vkCmdPushConstants(cmd, r.Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
			VkBuffer vbs[2] = { d.Mesh->Vertices.Handle, d.Mesh->Bones.Handle };
			VkDeviceSize offsets[2] = { 0, 0 };
			vkCmdBindVertexBuffers(cmd, 0, d.Skinned ? 2 : 1, vbs, offsets);
			vkCmdBindIndexBuffer(cmd, d.Mesh->Indices.Handle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, d.Mesh->IndexCount, 1, 0, 0, 0);
			m_Stats.DrawCalls++;
			m_Stats.Triangles += d.Mesh->IndexCount / 3;
		}
		m_Stats.Meshes = static_cast<uint32_t>(draws.size());

		if (view.DrawSky) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.SkyPipe); vkCmdDraw(cmd, 3, 1, 0, 0); }
		if (view.DrawGrid) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.GridPipe); vkCmdDraw(cmd, 3, 1, 0, 0); }
		vkCmdEndRendering(cmd);

		// Entity under the mouse (read when this frame slot comes back)
		if (view.PickX >= 0 && view.PickY >= 0 && view.PickX < static_cast<int>(view.Width) && view.PickY < static_cast<int>(view.Height)) {
			r.Entity->Transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			VkBufferImageCopy copy{};
			copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			// GL-order rows: row 0 is the bottom of the view, like the pick coordinates
			copy.imageOffset = { view.PickX, view.PickY, 0 };
			copy.imageExtent = { 1, 1, 1 };
			vkCmdCopyImageToBuffer(cmd, r.Entity->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fr.Readback.Handle, 1, &copy);
			fr.ReadbackPending = true;
		}
		else {
			m_PickedEntity = -1;
		}
		r.Color->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		r.Entity->Transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		r.Depth->Transition(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

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
		m_Stats.CpuMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
	}

}
