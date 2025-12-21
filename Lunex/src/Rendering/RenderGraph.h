#pragma once

/**
 * @file RenderGraph.h
 * @brief Frame graph system for automatic resource management and pass scheduling
 */

#include "Core/Core.h"
#include "RHI/RHI.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Lunex {

	// Forward declarations
	class RenderGraph;
	struct RenderGraphResourceDesc;

	// ============================================================================
	// RENDER GRAPH RESOURCE
	// ============================================================================
	
	/**
	 * @enum ResourceType
	 * @brief Type of resource in the render graph
	 */
	enum class RenderGraphResourceType {
		Texture,
		Buffer,
		External  // Imported from outside the graph
	};

	/**
	 * @struct RenderGraphTextureDesc
	 * @brief Description for a transient texture resource
	 */
	struct RenderGraphTextureDesc {
		uint32_t Width = 0;
		uint32_t Height = 0;
		RHI::TextureFormat Format = RHI::TextureFormat::RGBA8;
		uint32_t MipLevels = 1;
		bool IsRenderTarget = true;
		std::string DebugName;
		
		// Size policy
		enum class SizePolicy { Absolute, ScaleToSwapchain } Policy = SizePolicy::Absolute;
		float ScaleX = 1.0f;
		float ScaleY = 1.0f;
		
		static RenderGraphTextureDesc RenderTarget(const std::string& name, 
												   RHI::TextureFormat format = RHI::TextureFormat::RGBA8) {
			RenderGraphTextureDesc desc;
			desc.Format = format;
			desc.DebugName = name;
			desc.IsRenderTarget = true;
			return desc;
		}
		
		static RenderGraphTextureDesc ScaledRenderTarget(const std::string& name, 
														 float scale = 1.0f,
														 RHI::TextureFormat format = RHI::TextureFormat::RGBA8) {
			RenderGraphTextureDesc desc;
			desc.Policy = SizePolicy::ScaleToSwapchain;
			desc.ScaleX = scale;
			desc.ScaleY = scale;
			desc.Format = format;
			desc.DebugName = name;
			desc.IsRenderTarget = true;
			return desc;
		}
	};

	/**
	 * @struct RenderGraphBufferDesc
	 * @brief Description for a transient buffer resource
	 */
	struct RenderGraphBufferDesc {
		uint64_t Size = 0;
		RHI::BufferType Type = RHI::BufferType::Storage;
		RHI::BufferUsage Usage = RHI::BufferUsage::Dynamic;
		std::string DebugName;
	};

	/**
	 * @class RenderGraphResource
	 * @brief Handle to a resource in the render graph
	 */
	class RenderGraphResource {
	public:
		RenderGraphResource() = default;
		RenderGraphResource(uint32_t id, RenderGraphResourceType type)
			: m_ID(id), m_Type(type) {}
		
		uint32_t GetID() const { return m_ID; }
		RenderGraphResourceType GetType() const { return m_Type; }
		bool IsValid() const { return m_ID != UINT32_MAX; }
		
		bool operator==(const RenderGraphResource& other) const { return m_ID == other.m_ID; }
		bool operator!=(const RenderGraphResource& other) const { return m_ID != other.m_ID; }
		
	private:
		uint32_t m_ID = UINT32_MAX;
		RenderGraphResourceType m_Type = RenderGraphResourceType::Texture;
		
		friend class RenderGraph;
	};

	// ============================================================================
	// RENDER PASS
	// ============================================================================
	
	/**
	 * @enum PassType
	 * @brief Type of render pass
	 */
	enum class PassType {
		Graphics,
		Compute,
		Copy
	};

	/**
	 * @class RenderPassResources
	 * @brief Resources available to a render pass during execution
	 */
	class RenderPassResources {
	public:
		/**
		 * @brief Get a texture resource
		 */
		Ref<RHI::RHITexture2D> GetTexture(RenderGraphResource handle) const;
		
		/**
		 * @brief Get a buffer resource
		 */
		Ref<RHI::RHIBuffer> GetBuffer(RenderGraphResource handle) const;
		
		/**
		 * @brief Get render target (for graphics passes)
		 */
		Ref<RHI::RHIFramebuffer> GetRenderTarget() const { return m_RenderTarget; }
		
		/**
		 * @brief Get command list
		 */
		RHI::RHICommandList* GetCommandList() const { return m_CommandList; }
		
	private:
		friend class RenderGraph;
		
		std::unordered_map<uint32_t, Ref<RHI::RHITexture2D>> m_Textures;
		std::unordered_map<uint32_t, Ref<RHI::RHIBuffer>> m_Buffers;
		Ref<RHI::RHIFramebuffer> m_RenderTarget;
		RHI::RHICommandList* m_CommandList = nullptr;
	};

	/**
	 * @class RenderPassBuilder
	 * @brief Builder for declaring pass resources and dependencies
	 */
	class RenderPassBuilder {
	public:
		/**
		 * @brief Create a transient texture
		 */
		RenderGraphResource CreateTexture(const RenderGraphTextureDesc& desc);
		
		/**
		 * @brief Create a transient buffer
		 */
		RenderGraphResource CreateBuffer(const RenderGraphBufferDesc& desc);
		
		/**
		 * @brief Read from a texture
		 */
		RenderGraphResource ReadTexture(RenderGraphResource handle);
		
		/**
		 * @brief Write to a texture
		 */
		RenderGraphResource WriteTexture(RenderGraphResource handle);
		
		/**
		 * @brief Read from a buffer
		 */
		RenderGraphResource ReadBuffer(RenderGraphResource handle);
		
		/**
		 * @brief Write to a buffer
		 */
		RenderGraphResource WriteBuffer(RenderGraphResource handle);
		
		/**
		 * @brief Set color render target (graphics passes only)
		 */
		void SetRenderTarget(RenderGraphResource handle, uint32_t index = 0);
		
		/**
		 * @brief Set depth/stencil target (graphics passes only)
		 */
		void SetDepthTarget(RenderGraphResource handle);
		
		/**
		 * @brief Set pass name
		 */
		void SetName(const std::string& name) { m_PassName = name; }
		
	private:
		friend class RenderGraph;
		
		RenderGraph* m_Graph = nullptr;
		void* m_Pass = nullptr;  // RenderGraph::PassNode* - opaque to avoid forward decl issues
		std::string m_PassName;
		
		std::vector<RenderGraphResource> m_ColorTargets;
		RenderGraphResource m_DepthTarget;
		std::vector<RenderGraphResource> m_Reads;
		std::vector<RenderGraphResource> m_Writes;
	};

	/**
	 * @typedef RenderPassExecuteFunc
	 * @brief Function signature for pass execution
	 */
	using RenderPassExecuteFunc = std::function<void(const RenderPassResources&)>;

	/**
	 * @typedef RenderPassSetupFunc
	 * @brief Function signature for pass setup
	 */
	using RenderPassSetupFunc = std::function<void(RenderPassBuilder&)>;

	// ============================================================================
	// RENDER GRAPH
	// ============================================================================
	
	/**
	 * @class RenderGraph
	 * @brief Frame graph for automatic resource management and pass scheduling
	 * 
	 * Usage:
	 * 1. Create graph: RenderGraph graph;
	 * 2. Add passes: graph.AddPass(name, setup, execute);
	 * 3. Import external resources: graph.ImportTexture(...);
	 * 4. Compile: graph.Compile();
	 * 5. Execute: graph.Execute();
	 */
	class RenderGraph {
	public:
		RenderGraph();
		~RenderGraph();
		
		// ============================================
		// RESOURCE CREATION
		// ============================================
		
		/**
		 * @brief Import an external texture (from previous frame, swapchain, etc.)
		 */
		RenderGraphResource ImportTexture(const std::string& name, Ref<RHI::RHITexture2D> texture);
		
		/**
		 * @brief Import an external buffer
		 */
		RenderGraphResource ImportBuffer(const std::string& name, Ref<RHI::RHIBuffer> buffer);
		
		/**
		 * @brief Mark a resource as the final output
		 */
		void SetBackbufferSource(RenderGraphResource handle);
		
		// ============================================
		// PASS MANAGEMENT
		// ============================================
		
		/**
		 * @brief Add a graphics pass
		 */
		void AddPass(const std::string& name,
					const RenderPassSetupFunc& setup,
					const RenderPassExecuteFunc& execute);
		
		/**
		 * @brief Add a compute pass
		 */
		void AddComputePass(const std::string& name,
						   const RenderPassSetupFunc& setup,
						   const RenderPassExecuteFunc& execute);
		
		/**
		 * @brief Add a copy pass
		 */
		void AddCopyPass(const std::string& name,
						const RenderPassSetupFunc& setup,
						const RenderPassExecuteFunc& execute);
		
		// ============================================
		// EXECUTION
		// ============================================
		
		/**
		 * @brief Compile the graph (build dependency graph, cull passes, allocate resources)
		 */
		void Compile();
		
		/**
		 * @brief Execute all passes in dependency order
		 */
		void Execute();
		
		/**
		 * @brief Reset the graph for next frame
		 */
		void Reset();
		
		// ============================================
		// CONFIGURATION
		// ============================================
		
		/**
		 * @brief Set swapchain dimensions for scaled resources
		 */
		void SetSwapchainSize(uint32_t width, uint32_t height) {
			m_SwapchainWidth = width;
			m_SwapchainHeight = height;
		}
		
		/**
		 * @brief Enable/disable automatic culling of unused passes
		 */
		void SetEnablePassCulling(bool enable) { m_EnablePassCulling = enable; }
		
		// ============================================
		// DEBUG
		// ============================================
		
		/**
		 * @brief Export graph to GraphViz DOT format for visualization
		 */
		std::string ExportGraphViz() const;
		
		/**
		 * @brief Get statistics
		 */
		struct Statistics {
			uint32_t TotalPasses = 0;
			uint32_t ExecutedPasses = 0;
			uint32_t CulledPasses = 0;
			uint32_t TransientResources = 0;
			uint64_t TransientMemoryUsage = 0;
		};
		
		const Statistics& GetStatistics() const { return m_Stats; }
		
	private:
		friend class RenderPassBuilder;
		
		// Internal data structures
		struct ResourceNode;
		struct PassNode;
		
		void CompilePasses();
		void CullUnusedPasses();
		void AllocateResources();
		void ExecutePass(PassNode* pass);
		
		std::vector<Scope<PassNode>> m_Passes;
		std::vector<Scope<ResourceNode>> m_Resources;
		std::unordered_map<std::string, uint32_t> m_ResourceNameMap;
		
		uint32_t m_SwapchainWidth = 1920;
		uint32_t m_SwapchainHeight = 1080;
		bool m_EnablePassCulling = true;
		bool m_Compiled = false;
		
		RenderGraphResource m_BackbufferSource;
		
		Statistics m_Stats;
		
		Ref<RHI::RHICommandList> m_CommandList;
	};

} // namespace Lunex
