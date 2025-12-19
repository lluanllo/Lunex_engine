#pragma once

/**
 * @file RHIPipeline.h
 * @brief Graphics and compute pipeline state objects
 * 
 * A Pipeline encapsulates the complete GPU state needed for rendering:
 * - Shader program
 * - Vertex input layout
 * - Rasterizer state (culling, fill mode)
 * - Depth/stencil state
 * - Blend state
 * - Primitive topology
 * 
 * Pipelines are immutable after creation for performance.
 */

#include "RHIResource.h"
#include "RHITypes.h"
#include "RHIBuffer.h"

namespace Lunex {
namespace RHI {

	// Forward declarations
	class RHIShader;

	// ============================================================================
	// PIPELINE DESCRIPTION
	// ============================================================================
	
	/**
	 * @struct GraphicsPipelineDesc
	 * @brief Complete description of a graphics pipeline
	 */
	struct GraphicsPipelineDesc {
		// Shader
		Ref<RHIShader> Shader;
		
		// Vertex input
		VertexLayout VertexLayout;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		
		// Fixed-function states
		RasterizerState Rasterizer;
		DepthStencilState DepthStencil;
		BlendState Blend;
		
		// Render target info (for validation)
		std::vector<TextureFormat> ColorFormats;
		TextureFormat DepthFormat = TextureFormat::None;
		uint32_t SampleCount = 1;
		
		// Debug name
		std::string DebugName;
		
		// ============================================
		// BUILDER HELPERS
		// ============================================
		
		GraphicsPipelineDesc& SetShader(Ref<RHIShader> shader) {
			Shader = shader;
			return *this;
		}
		
		GraphicsPipelineDesc& SetVertexLayout(const RHI::VertexLayout& layout) {
			VertexLayout = layout;
			return *this;
		}
		
		GraphicsPipelineDesc& SetTopology(PrimitiveTopology topology) {
			Topology = topology;
			return *this;
		}
		
		GraphicsPipelineDesc& SetRasterizer(const RasterizerState& state) {
			Rasterizer = state;
			return *this;
		}
		
		GraphicsPipelineDesc& SetDepthStencil(const DepthStencilState& state) {
			DepthStencil = state;
			return *this;
		}
		
		GraphicsPipelineDesc& SetBlend(const BlendState& state) {
			Blend = state;
			return *this;
		}
		
		GraphicsPipelineDesc& SetCullMode(CullMode mode) {
			Rasterizer.Culling = mode;
			return *this;
		}
		
		GraphicsPipelineDesc& SetFillMode(FillMode mode) {
			Rasterizer.Fill = mode;
			return *this;
		}
		
		GraphicsPipelineDesc& EnableDepthTest(bool enable = true) {
			DepthStencil.DepthTestEnabled = enable;
			return *this;
		}
		
		GraphicsPipelineDesc& EnableDepthWrite(bool enable = true) {
			DepthStencil.DepthWriteEnabled = enable;
			return *this;
		}
		
		GraphicsPipelineDesc& SetDepthFunc(CompareFunc func) {
			DepthStencil.DepthCompareFunc = func;
			return *this;
		}
		
		GraphicsPipelineDesc& EnableBlend(bool enable = true) {
			Blend.Enabled = enable;
			return *this;
		}
		
		GraphicsPipelineDesc& AddColorFormat(TextureFormat format) {
			ColorFormats.push_back(format);
			return *this;
		}
		
		GraphicsPipelineDesc& SetDepthFormat(TextureFormat format) {
			DepthFormat = format;
			return *this;
		}
		
		GraphicsPipelineDesc& SetName(const std::string& name) {
			DebugName = name;
			return *this;
		}
	};

	/**
	 * @struct ComputePipelineDesc
	 * @brief Description of a compute pipeline
	 */
	struct ComputePipelineDesc {
		Ref<RHIShader> Shader;
		std::string DebugName;
		
		ComputePipelineDesc& SetShader(Ref<RHIShader> shader) {
			Shader = shader;
			return *this;
		}
		
		ComputePipelineDesc& SetName(const std::string& name) {
			DebugName = name;
			return *this;
		}
	};

	// ============================================================================
	// RHI GRAPHICS PIPELINE
	// ============================================================================
	
	/**
	 * @class RHIGraphicsPipeline
	 * @brief Immutable graphics pipeline state object
	 */
	class RHIGraphicsPipeline : public RHIResource {
	public:
		virtual ~RHIGraphicsPipeline() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Pipeline; }
		
		// ============================================
		// PIPELINE INFO
		// ============================================
		
		/**
		 * @brief Get the pipeline description
		 */
		const GraphicsPipelineDesc& GetDesc() const { return m_Desc; }
		
		/**
		 * @brief Get the shader used by this pipeline
		 */
		Ref<RHIShader> GetShader() const { return m_Desc.Shader; }
		
		/**
		 * @brief Get the vertex layout
		 */
		const VertexLayout& GetVertexLayout() const { return m_Desc.VertexLayout; }
		
		/**
		 * @brief Get primitive topology
		 */
		PrimitiveTopology GetTopology() const { return m_Desc.Topology; }
		
		/**
		 * @brief Get rasterizer state
		 */
		const RasterizerState& GetRasterizerState() const { return m_Desc.Rasterizer; }
		
		/**
		 * @brief Get depth/stencil state
		 */
		const DepthStencilState& GetDepthStencilState() const { return m_Desc.DepthStencil; }
		
		/**
		 * @brief Get blend state
		 */
		const BlendState& GetBlendState() const { return m_Desc.Blend; }
		
		// ============================================
		// BINDING (OpenGL-style for compatibility)
		// ============================================
		
		/**
		 * @brief Bind the pipeline (sets all states)
		 */
		virtual void Bind() const = 0;
		
		/**
		 * @brief Unbind the pipeline
		 */
		virtual void Unbind() const = 0;
		
		// ============================================
		// GPU MEMORY
		// ============================================
		
		uint64_t GetGPUMemorySize() const override { return 0; }  // Pipelines use minimal memory
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create a graphics pipeline
		 * @param desc Pipeline description
		 */
		static Ref<RHIGraphicsPipeline> Create(const GraphicsPipelineDesc& desc);
		
	protected:
		GraphicsPipelineDesc m_Desc;
	};

	// ============================================================================
	// RHI COMPUTE PIPELINE
	// ============================================================================
	
	/**
	 * @class RHIComputePipeline
	 * @brief Immutable compute pipeline state object
	 */
	class RHIComputePipeline : public RHIResource {
	public:
		virtual ~RHIComputePipeline() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Pipeline; }
		
		// ============================================
		// PIPELINE INFO
		// ============================================
		
		/**
		 * @brief Get the pipeline description
		 */
		const ComputePipelineDesc& GetDesc() const { return m_Desc; }
		
		/**
		 * @brief Get the compute shader
		 */
		Ref<RHIShader> GetShader() const { return m_Desc.Shader; }
		
		/**
		 * @brief Get the work group size
		 */
		virtual void GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const = 0;
		
		// ============================================
		// BINDING
		// ============================================
		
		/**
		 * @brief Bind the compute pipeline
		 */
		virtual void Bind() const = 0;
		
		/**
		 * @brief Dispatch compute work
		 */
		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const = 0;
		
		/**
		 * @brief Dispatch with automatic group calculation
		 * @param totalX Total items in X dimension
		 * @param totalY Total items in Y dimension
		 * @param totalZ Total items in Z dimension
		 */
		void DispatchAuto(uint32_t totalX, uint32_t totalY = 1, uint32_t totalZ = 1) const {
			uint32_t sizeX, sizeY, sizeZ;
			GetWorkGroupSize(sizeX, sizeY, sizeZ);
			
			uint32_t groupsX = (totalX + sizeX - 1) / sizeX;
			uint32_t groupsY = (totalY + sizeY - 1) / sizeY;
			uint32_t groupsZ = (totalZ + sizeZ - 1) / sizeZ;
			
			Dispatch(groupsX, groupsY, groupsZ);
		}
		
		// ============================================
		// GPU MEMORY
		// ============================================
		
		uint64_t GetGPUMemorySize() const override { return 0; }
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create a compute pipeline
		 * @param desc Pipeline description
		 */
		static Ref<RHIComputePipeline> Create(const ComputePipelineDesc& desc);
		
	protected:
		ComputePipelineDesc m_Desc;
	};

	// ============================================================================
	// PIPELINE CACHE
	// ============================================================================
	
	/**
	 * @class PipelineCache
	 * @brief Caches compiled pipelines to avoid redundant creation
	 * 
	 * Pipeline compilation can be expensive. This cache stores compiled
	 * pipelines keyed by their description hash.
	 */
	class PipelineCache {
	public:
		static PipelineCache& Get();
		
		/**
		 * @brief Get or create a graphics pipeline
		 */
		Ref<RHIGraphicsPipeline> GetGraphicsPipeline(const GraphicsPipelineDesc& desc);
		
		/**
		 * @brief Get or create a compute pipeline
		 */
		Ref<RHIComputePipeline> GetComputePipeline(const ComputePipelineDesc& desc);
		
		/**
		 * @brief Clear all cached pipelines
		 */
		void Clear();
		
		/**
		 * @brief Get statistics
		 */
		size_t GetCacheSize() const { return m_GraphicsPipelines.size() + m_ComputePipelines.size(); }
		size_t GetCacheHits() const { return m_CacheHits; }
		size_t GetCacheMisses() const { return m_CacheMisses; }
		
	private:
		PipelineCache() = default;
		
		size_t HashGraphicsDesc(const GraphicsPipelineDesc& desc) const;
		size_t HashComputeDesc(const ComputePipelineDesc& desc) const;
		
		std::unordered_map<size_t, Ref<RHIGraphicsPipeline>> m_GraphicsPipelines;
		std::unordered_map<size_t, Ref<RHIComputePipeline>> m_ComputePipelines;
		
		mutable size_t m_CacheHits = 0;
		mutable size_t m_CacheMisses = 0;
	};

} // namespace RHI
} // namespace Lunex
