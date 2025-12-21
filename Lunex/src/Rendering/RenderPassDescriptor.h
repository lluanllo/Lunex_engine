#pragma once

/**
 * @file RenderPassDescriptor.h
 * @brief Data-driven render pass description system (AAA-level)
 * 
 * This replaces rigid class-based passes with configurable descriptions.
 * Inspired by Frostbite's Frame Graph and Unreal's RDG.
 */

#include "Core/Core.h"
#include "RHI/RHI.h"
#include "RenderGraph.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <variant>

namespace Lunex {

	// Forward declarations
	class Scene;
	struct SceneRenderInfo;

	// ============================================================================
	// RESOURCE DEPENDENCY
	// ============================================================================
	
	/**
	 * @enum ResourceAccess
	 * @brief How a pass accesses a resource
	 */
	enum class ResourceAccess {
		Read,           // Read-only access
		Write,          // Write-only access (creates new version)
		ReadWrite,      // Read-modify-write
		RenderTarget,   // Used as render target attachment
		DepthTarget,    // Used as depth/stencil attachment
		UAV             // Unordered Access View (compute)
	};

	/**
	 * @struct ResourceDependency
	 * @brief Declares a resource dependency for a pass
	 */
	struct ResourceDependency {
		std::string Name;                           // Resource name
		ResourceAccess Access = ResourceAccess::Read;
		
		// For creation (if this is the first writer)
		RenderGraphTextureDesc TextureDesc;
		RenderGraphBufferDesc BufferDesc;
		bool IsTexture = true;
		
		// Binding info
		uint32_t Slot = 0;                          // Texture slot or buffer binding
		
		// Factory helpers
		static ResourceDependency ReadTexture(const std::string& name, uint32_t slot = 0) {
			ResourceDependency dep;
			dep.Name = name;
			dep.Access = ResourceAccess::Read;
			dep.Slot = slot;
			dep.IsTexture = true;
			return dep;
		}
		
		static ResourceDependency WriteTexture(const std::string& name, 
											   RHI::TextureFormat format = RHI::TextureFormat::RGBA8) {
			ResourceDependency dep;
			dep.Name = name;
			dep.Access = ResourceAccess::Write;
			dep.IsTexture = true;
			dep.TextureDesc = RenderGraphTextureDesc::ScaledRenderTarget(name, 1.0f, format);
			return dep;
		}
		
		static ResourceDependency RenderTarget(const std::string& name, 
											   uint32_t slot = 0,
											   RHI::TextureFormat format = RHI::TextureFormat::RGBA8) {
			ResourceDependency dep;
			dep.Name = name;
			dep.Access = ResourceAccess::RenderTarget;
			dep.Slot = slot;
			dep.IsTexture = true;
			dep.TextureDesc = RenderGraphTextureDesc::ScaledRenderTarget(name, 1.0f, format);
			return dep;
		}
		
		static ResourceDependency DepthTarget(const std::string& name) {
			ResourceDependency dep;
			dep.Name = name;
			dep.Access = ResourceAccess::DepthTarget;
			dep.IsTexture = true;
			dep.TextureDesc = RenderGraphTextureDesc::ScaledRenderTarget(name, 1.0f, RHI::TextureFormat::Depth24Stencil8);
			return dep;
		}
		
		static ResourceDependency ReadBuffer(const std::string& name, uint32_t slot = 0) {
			ResourceDependency dep;
			dep.Name = name;
			dep.Access = ResourceAccess::Read;
			dep.Slot = slot;
			dep.IsTexture = false;
			return dep;
		}
		
		static ResourceDependency WriteBuffer(const std::string& name, uint64_t size, RHI::BufferType type) {
			ResourceDependency dep;
			dep.Name = name;
			dep.Access = ResourceAccess::Write;
			dep.IsTexture = false;
			dep.BufferDesc.Size = size;
			dep.BufferDesc.Type = type;
			dep.BufferDesc.DebugName = name;
			return dep;
		}
	};

	// ============================================================================
	// PIPELINE STATE
	// ============================================================================
	
	/**
	 * @struct PassPipelineState
	 * @brief Pipeline configuration for a pass
	 */
	struct PassPipelineState {
		// Shaders (paths)
		std::string VertexShader;
		std::string PixelShader;
		std::string ComputeShader;  // For compute passes
		
		// Rasterizer state
		bool WireframeMode = false;
		bool CullBackFaces = true;
		bool FrontFaceCCW = true;
		
		// Depth state
		bool DepthTestEnabled = true;
		bool DepthWriteEnabled = true;
		RHI::CompareFunc DepthCompareOp = RHI::CompareFunc::Less;
		
		// Blend state
		bool BlendEnabled = false;
		RHI::BlendFactor SrcBlend = RHI::BlendFactor::One;
		RHI::BlendFactor DstBlend = RHI::BlendFactor::Zero;
		
		// Clear settings
		bool ClearColor = true;
		bool ClearDepth = true;
		glm::vec4 ClearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		float ClearDepthValue = 1.0f;
	};

	// ============================================================================
	// RENDER PASS DESCRIPTOR
	// ============================================================================
	
	/**
	 * @enum PassCategory
	 * @brief Category for pass grouping and ordering hints
	 */
	enum class PassCategory {
		PrePass,        // Shadow maps, depth prepass
		GBuffer,        // Deferred geometry
		Lighting,       // Light accumulation
		ForwardOpaque,  // Forward rendered opaques
		ForwardTransparent,
		PostProcess,    // Post-processing effects
		UI,             // User interface
		Debug           // Debug overlays
	};

	/**
	 * @typedef PassExecuteFunction
	 * @brief Execution function signature with scene info
	 */
	using PassExecuteFunction = std::function<void(
		RHI::RHICommandList& cmdList,
		const RenderPassResources& resources,
		const SceneRenderInfo& sceneInfo
	)>;

	/**
	 * @typedef PassConditionFunction
	 * @brief Condition function to determine if pass should execute
	 */
	using PassConditionFunction = std::function<bool(const SceneRenderInfo& sceneInfo)>;

	/**
	 * @struct RenderPassDescriptor
	 * @brief Complete data-driven description of a render pass
	 * 
	 * Usage:
	 * ```cpp
	 * RenderPassDescriptor desc;
	 * desc.Name = "GBuffer";
	 * desc.Category = PassCategory::GBuffer;
	 * desc.Inputs = { 
	 *     ResourceDependency::ReadTexture("ShadowMap", 0) 
	 * };
	 * desc.Outputs = {
	 *     ResourceDependency::RenderTarget("GBuffer_Albedo", 0, TextureFormat::RGBA8),
	 *     ResourceDependency::RenderTarget("GBuffer_Normal", 1, TextureFormat::RGBA16F),
	 *     ResourceDependency::DepthTarget("GBuffer_Depth")
	 * };
	 * desc.Execute = [](RHICommandList& cmd, const Resources& res, const SceneInfo& scene) {
	 *     DrawOpaqueGeometry(cmd, scene);
	 * };
	 * 
	 * renderSystem.RegisterPass(desc);
	 * ```
	 */
	struct RenderPassDescriptor {
		// ==================== IDENTIFICATION ====================
		std::string Name;                           // Unique pass name
		PassCategory Category = PassCategory::ForwardOpaque;
		int32_t Priority = 0;                       // Ordering within category (lower = earlier)
		
		// ==================== DEPENDENCIES ====================
		std::vector<ResourceDependency> Inputs;     // Resources this pass reads
		std::vector<ResourceDependency> Outputs;    // Resources this pass writes/creates
		
		// ==================== PIPELINE ====================
		PassPipelineState Pipeline;
		
		// ==================== EXECUTION ====================
		PassExecuteFunction Execute;                // The actual rendering logic
		PassConditionFunction Condition;            // Optional: skip pass if returns false
		
		// ==================== FLAGS ====================
		bool Enabled = true;                        // Can be toggled at runtime
		bool IsCompute = false;                     // Compute pass vs graphics pass
		bool AllowAsyncCompute = false;             // Can run on async compute queue
		
		// ==================== DEBUG ====================
		std::string Description;                    // Human-readable description
		glm::vec4 DebugColor = glm::vec4(1.0f);    // Color in RenderDoc/profiler
		
		// ==================== FACTORY METHODS ====================
		
		/**
		 * @brief Create a simple graphics pass descriptor
		 */
		static RenderPassDescriptor Graphics(
			const std::string& name,
			PassCategory category,
			std::vector<ResourceDependency> inputs,
			std::vector<ResourceDependency> outputs,
			PassExecuteFunction execute
		) {
			RenderPassDescriptor desc;
			desc.Name = name;
			desc.Category = category;
			desc.Inputs = std::move(inputs);
			desc.Outputs = std::move(outputs);
			desc.Execute = std::move(execute);
			desc.IsCompute = false;
			return desc;
		}
		
		/**
		 * @brief Create a compute pass descriptor
		 */
		static RenderPassDescriptor Compute(
			const std::string& name,
			std::vector<ResourceDependency> inputs,
			std::vector<ResourceDependency> outputs,
			PassExecuteFunction execute,
			bool allowAsync = false
		) {
			RenderPassDescriptor desc;
			desc.Name = name;
			desc.Category = PassCategory::PostProcess;  // Most compute is post-process
			desc.Inputs = std::move(inputs);
			desc.Outputs = std::move(outputs);
			desc.Execute = std::move(execute);
			desc.IsCompute = true;
			desc.AllowAsyncCompute = allowAsync;
			return desc;
		}
		
		/**
		 * @brief Create a fullscreen post-process pass
		 */
		static RenderPassDescriptor PostProcess(
			const std::string& name,
			const std::string& inputTexture,
			const std::string& outputTexture,
			PassExecuteFunction execute
		) {
			RenderPassDescriptor desc;
			desc.Name = name;
			desc.Category = PassCategory::PostProcess;
			desc.Inputs = { ResourceDependency::ReadTexture(inputTexture, 0) };
			desc.Outputs = { ResourceDependency::RenderTarget(outputTexture, 0) };
			desc.Execute = std::move(execute);
			desc.Pipeline.DepthTestEnabled = false;
			desc.Pipeline.DepthWriteEnabled = false;
			return desc;
		}
	};

	// ============================================================================
	// PASS REGISTRY
	// ============================================================================
	
	/**
	 * @class PassRegistry
	 * @brief Global registry for render pass descriptors
	 * 
	 * Allows modular pass registration from anywhere in the codebase.
	 * Passes are automatically discovered and ordered by the RenderSystem.
	 */
	class PassRegistry {
	public:
		static PassRegistry& Get() {
			static PassRegistry instance;
			return instance;
		}
		
		/**
		 * @brief Register a pass descriptor
		 */
		void Register(const RenderPassDescriptor& descriptor) {
			m_Passes[descriptor.Name] = descriptor;
		}
		
		/**
		 * @brief Unregister a pass
		 */
		void Unregister(const std::string& name) {
			m_Passes.erase(name);
		}
		
		/**
		 * @brief Get a registered pass
		 */
		const RenderPassDescriptor* GetPass(const std::string& name) const {
			auto it = m_Passes.find(name);
			return it != m_Passes.end() ? &it->second : nullptr;
		}
		
		/**
		 * @brief Get all registered passes
		 */
		const std::unordered_map<std::string, RenderPassDescriptor>& GetAllPasses() const {
			return m_Passes;
		}
		
		/**
		 * @brief Get passes sorted by category and priority
		 */
		std::vector<const RenderPassDescriptor*> GetSortedPasses() const {
			std::vector<const RenderPassDescriptor*> sorted;
			sorted.reserve(m_Passes.size());
			
			for (const auto& [name, desc] : m_Passes) {
				if (desc.Enabled) {
					sorted.push_back(&desc);
				}
			}
			
			std::sort(sorted.begin(), sorted.end(), 
				[](const RenderPassDescriptor* a, const RenderPassDescriptor* b) {
					if (a->Category != b->Category) {
						return static_cast<int>(a->Category) < static_cast<int>(b->Category);
					}
					return a->Priority < b->Priority;
				}
			);
			
			return sorted;
		}
		
		/**
		 * @brief Clear all passes
		 */
		void Clear() {
			m_Passes.clear();
		}
		
	private:
		PassRegistry() = default;
		std::unordered_map<std::string, RenderPassDescriptor> m_Passes;
	};

	// ============================================================================
	// HELPER MACROS FOR PASS REGISTRATION
	// ============================================================================
	
	/**
	 * @def REGISTER_RENDER_PASS
	 * @brief Macro for automatic pass registration at startup
	 * 
	 * Usage:
	 * ```cpp
	 * REGISTER_RENDER_PASS(MyGeometryPass) {
	 *     return RenderPassDescriptor::Graphics(
	 *         "Geometry",
	 *         PassCategory::GBuffer,
	 *         { ... },
	 *         { ... },
	 *         [](auto& cmd, auto& res, auto& scene) { ... }
	 *     );
	 * }
	 * ```
	 */
	#define REGISTER_RENDER_PASS(Name) \
		static struct Name##_PassRegistrar { \
			Name##_PassRegistrar() { \
				PassRegistry::Get().Register(CreateDescriptor()); \
			} \
			static RenderPassDescriptor CreateDescriptor(); \
		} s_##Name##_Registrar; \
		RenderPassDescriptor Name##_PassRegistrar::CreateDescriptor()

} // namespace Lunex
