#pragma once

/**
 * @file RHICommandList.h
 * @brief GPU command recording and execution interface
 * 
 * Command lists record GPU operations for deferred execution.
 * This enables:
 * - Multithreaded command recording
 * - Command list reuse
 * - Efficient batching
 * 
 * For OpenGL: Commands execute immediately (simulated command list)
 * For Vulkan/DX12: True deferred command buffers
 */

#include "RHIResource.h"
#include "RHITypes.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"

namespace Lunex {
namespace RHI {

	// Forward declarations
	class RHIFramebuffer;
	class RHITexture2D;
	class RHITextureCube;
	class RHISampler;

	// ============================================================================
	// DRAW ARGUMENTS
	// ============================================================================
	
	/**
	 * @struct DrawArgs
	 * @brief Arguments for indexed draw call
	 */
	struct DrawArgs {
		uint32_t IndexCount = 0;
		uint32_t InstanceCount = 1;
		uint32_t FirstIndex = 0;
		int32_t VertexOffset = 0;
		uint32_t FirstInstance = 0;
	};

	/**
	 * @struct DrawArrayArgs
	 * @brief Arguments for non-indexed draw call
	 */
	struct DrawArrayArgs {
		uint32_t VertexCount = 0;
		uint32_t InstanceCount = 1;
		uint32_t FirstVertex = 0;
		uint32_t FirstInstance = 0;
	};

	/**
	 * @struct IndirectDrawArgs
	 * @brief Arguments for indirect draw (stored in GPU buffer)
	 */
	struct IndirectDrawArgs {
		uint32_t IndexCount;
		uint32_t InstanceCount;
		uint32_t FirstIndex;
		int32_t VertexOffset;
		uint32_t FirstInstance;
	};

	// ============================================================================
	// RESOURCE BARRIER
	// ============================================================================
	
	/**
	 * @struct ResourceBarrier
	 * @brief Describes a resource state transition
	 */
	struct ResourceBarrier {
		RHIResource* Resource = nullptr;
		ResourceState StateBefore = ResourceState::Undefined;
		ResourceState StateAfter = ResourceState::Undefined;
		
		static ResourceBarrier Transition(RHIResource* res, ResourceState before, ResourceState after) {
			return { res, before, after };
		}
	};

	// ============================================================================
	// VIEWPORT / SCISSOR
	// ============================================================================
	
	struct ViewportScissor {
		Viewport Viewport;
		ScissorRect Scissor;
		bool ScissorEnabled = false;
	};

	// ============================================================================
	// RENDER PASS BEGIN INFO
	// ============================================================================
	
	/**
	 * @struct RenderPassBeginInfo
	 * @brief Configuration for beginning a render pass
	 */
	struct RenderPassBeginInfo {
		RHIFramebuffer* Framebuffer = nullptr;
		std::vector<ClearValue> ClearValues;
		bool ClearColor = true;
		bool ClearDepth = true;
		bool ClearStencil = false;
		
		Viewport RenderViewport;
		ScissorRect RenderScissor;
		bool UseScissor = false;
	};

	// ============================================================================
	// RHI COMMAND LIST
	// ============================================================================
	
	/**
	 * @class RHICommandList
	 * @brief Records GPU commands for later execution
	 * 
	 * Usage pattern:
	 * 1. Begin()
	 * 2. Record commands (SetPipeline, SetVertexBuffer, Draw, etc.)
	 * 3. End()
	 * 4. Submit to queue
	 */
	class RHICommandList : public RHIResource {
	public:
		virtual ~RHICommandList() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::CommandList; }
		
		// ============================================
		// COMMAND LIST LIFECYCLE
		// ============================================
		
		/**
		 * @brief Begin recording commands
		 */
		virtual void Begin() = 0;
		
		/**
		 * @brief End recording commands
		 */
		virtual void End() = 0;
		
		/**
		 * @brief Reset the command list for reuse
		 */
		virtual void Reset() = 0;
		
		/**
		 * @brief Check if currently recording
		 */
		virtual bool IsRecording() const = 0;
		
		// ============================================
		// RENDER PASS
		// ============================================
		
		/**
		 * @brief Begin a render pass
		 * @param info Render pass configuration
		 */
		virtual void BeginRenderPass(const RenderPassBeginInfo& info) = 0;
		
		/**
		 * @brief End the current render pass
		 */
		virtual void EndRenderPass() = 0;
		
		// ============================================
		// PIPELINE STATE
		// ============================================
		
		/**
		 * @brief Bind a graphics pipeline
		 */
		virtual void SetPipeline(const RHIGraphicsPipeline* pipeline) = 0;
		
		/**
		 * @brief Bind a compute pipeline
		 */
		virtual void SetComputePipeline(const RHIComputePipeline* pipeline) = 0;
		
		// ============================================
		// VIEWPORT / SCISSOR
		// ============================================
		
		/**
		 * @brief Set viewport
		 */
		virtual void SetViewport(const Viewport& viewport) = 0;
		
		/**
		 * @brief Set viewport from dimensions
		 */
		void SetViewport(float x, float y, float width, float height) {
			Viewport vp = { x, y, width, height, 0.0f, 1.0f };
			SetViewport(vp);
		}
		
		/**
		 * @brief Set scissor rectangle
		 */
		virtual void SetScissor(const ScissorRect& scissor) = 0;
		
		/**
		 * @brief Set scissor from dimensions
		 */
		void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) {
			ScissorRect sc = { x, y, width, height };
			SetScissor(sc);
		}
		
		// ============================================
		// VERTEX / INDEX BUFFERS
		// ============================================
		
		/**
		 * @brief Bind vertex buffer
		 * @param buffer Vertex buffer to bind
		 * @param slot Binding slot (for multiple vertex streams)
		 * @param offset Byte offset into buffer
		 */
		virtual void SetVertexBuffer(const RHIBuffer* buffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
		
		/**
		 * @brief Bind multiple vertex buffers
		 */
		virtual void SetVertexBuffers(const RHIBuffer* const* buffers, uint32_t count, 
									  const uint64_t* offsets = nullptr) = 0;
		
		/**
		 * @brief Bind index buffer
		 * @param buffer Index buffer
		 * @param offset Byte offset
		 */
		virtual void SetIndexBuffer(const RHIBuffer* buffer, uint64_t offset = 0) = 0;
		
		// ============================================
		// UNIFORM / CONSTANT BUFFERS
		// ============================================
		
		/**
		 * @brief Bind uniform buffer
		 * @param buffer Uniform buffer
		 * @param binding Shader binding point
		 * @param stages Which shader stages can access
		 */
		virtual void SetUniformBuffer(const RHIBuffer* buffer, uint32_t binding, 
									  ShaderStage stages = ShaderStage::AllGraphics) = 0;
		
		/**
		 * @brief Bind uniform buffer range
		 */
		virtual void SetUniformBufferRange(const RHIBuffer* buffer, uint32_t binding,
										   uint64_t offset, uint64_t size,
										   ShaderStage stages = ShaderStage::AllGraphics) = 0;
		
		/**
		 * @brief Bind storage buffer (SSBO)
		 */
		virtual void SetStorageBuffer(const RHIBuffer* buffer, uint32_t binding,
									  ShaderStage stages = ShaderStage::Compute) = 0;
		
		// ============================================
		// TEXTURES / SAMPLERS
		// ============================================
		
		/**
		 * @brief Bind texture
		 * @param texture Texture to bind
		 * @param slot Texture slot
		 */
		virtual void SetTexture(const RHITexture* texture, uint32_t slot) = 0;
		
		/**
		 * @brief Bind sampler
		 * @param sampler Sampler state
		 * @param slot Sampler slot
		 */
		virtual void SetSampler(const RHISampler* sampler, uint32_t slot) = 0;
		
		/**
		 * @brief Bind texture and sampler together
		 */
		virtual void SetTextureAndSampler(const RHITexture* texture, const RHISampler* sampler, 
										  uint32_t slot) = 0;
		
		/**
		 * @brief Bind texture for compute shader access (image load/store)
		 */
		virtual void SetStorageTexture(const RHITexture* texture, uint32_t slot,
									   BufferAccess access = BufferAccess::ReadWrite) = 0;
		
		// ============================================
		// DRAW COMMANDS
		// ============================================
		
		/**
		 * @brief Draw indexed primitives
		 */
		virtual void DrawIndexed(const DrawArgs& args) = 0;
		
		/**
		 * @brief Draw indexed primitives (convenience)
		 */
		void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, 
						 uint32_t firstIndex = 0, int32_t vertexOffset = 0) {
			DrawArgs args = { indexCount, instanceCount, firstIndex, vertexOffset, 0 };
			DrawIndexed(args);
		}
		
		/**
		 * @brief Draw non-indexed primitives
		 */
		virtual void Draw(const DrawArrayArgs& args) = 0;
		
		/**
		 * @brief Draw non-indexed primitives (convenience)
		 */
		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) {
			DrawArrayArgs args = { vertexCount, instanceCount, firstVertex, 0 };
			Draw(args);
		}
		
		/**
		 * @brief Indirect draw from GPU buffer
		 */
		virtual void DrawIndexedIndirect(const RHIBuffer* argsBuffer, uint64_t offset = 0) = 0;
		
		/**
		 * @brief Multi-draw indirect
		 */
		virtual void DrawIndexedIndirectCount(const RHIBuffer* argsBuffer, 
											  const RHIBuffer* countBuffer,
											  uint64_t argsOffset, uint64_t countOffset,
											  uint32_t maxDrawCount) = 0;
		
		// ============================================
		// COMPUTE DISPATCH
		// ============================================
		
		/**
		 * @brief Dispatch compute work groups
		 */
		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) = 0;
		
		/**
		 * @brief Indirect compute dispatch
		 */
		virtual void DispatchIndirect(const RHIBuffer* argsBuffer, uint64_t offset = 0) = 0;
		
		// ============================================
		// RESOURCE BARRIERS
		// ============================================
		
		/**
		 * @brief Insert resource barriers
		 */
		virtual void ResourceBarriers(const ResourceBarrier* barriers, uint32_t count) = 0;
		
		/**
		 * @brief Insert a single barrier
		 */
		void ResourceBarrier(const ResourceBarrier& barrier) {
			ResourceBarriers(&barrier, 1);
		}
		
		/**
		 * @brief Memory barrier for compute shaders
		 */
		virtual void MemoryBarrier() = 0;
		
		// ============================================
		// COPY OPERATIONS
		// ============================================
		
		/**
		 * @brief Copy buffer to buffer
		 */
		virtual void CopyBuffer(const RHIBuffer* src, RHIBuffer* dst,
								uint64_t srcOffset, uint64_t dstOffset, uint64_t size) = 0;
		
		/**
		 * @brief Copy texture to texture
		 */
		virtual void CopyTexture(const RHITexture* src, RHITexture* dst,
								 const TextureRegion& srcRegion, const TextureRegion& dstRegion) = 0;
		
		/**
		 * @brief Copy buffer to texture
		 */
		virtual void CopyBufferToTexture(const RHIBuffer* src, RHITexture* dst,
										 uint64_t bufferOffset, const TextureRegion& textureRegion) = 0;
		
		/**
		 * @brief Copy texture to buffer
		 */
		virtual void CopyTextureToBuffer(const RHITexture* src, RHIBuffer* dst,
										 const TextureRegion& textureRegion, uint64_t bufferOffset) = 0;
		
		// ============================================
		// CLEAR OPERATIONS
		// ============================================
		
		/**
		 * @brief Clear render target
		 */
		virtual void ClearRenderTarget(RHITexture2D* texture, const ClearValue& value) = 0;
		
		/**
		 * @brief Clear depth/stencil
		 */
		virtual void ClearDepthStencil(RHITexture2D* texture, float depth, uint8_t stencil) = 0;
		
		// ============================================
		// DEBUG
		// ============================================
		
		/**
		 * @brief Begin debug event (shows in RenderDoc, PIX, etc.)
		 */
		virtual void BeginDebugEvent(const std::string& name) = 0;
		
		/**
		 * @brief End debug event
		 */
		virtual void EndDebugEvent() = 0;
		
		/**
		 * @brief Insert debug marker
		 */
		virtual void InsertDebugMarker(const std::string& name) = 0;
		
		// ============================================
		// GPU MEMORY
		// ============================================
		
		uint64_t GetGPUMemorySize() const override { return 0; }
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create a graphics command list
		 */
		static Ref<RHICommandList> CreateGraphics();
		
		/**
		 * @brief Create a compute command list
		 */
		static Ref<RHICommandList> CreateCompute();
		
		/**
		 * @brief Create a copy/transfer command list
		 */
		static Ref<RHICommandList> CreateCopy();
	};

	// ============================================================================
	// SCOPED DEBUG EVENT
	// ============================================================================
	
	/**
	 * @class ScopedDebugEvent
	 * @brief RAII helper for GPU debug events
	 */
	class ScopedDebugEvent {
	public:
		ScopedDebugEvent(RHICommandList* cmdList, const std::string& name)
			: m_CommandList(cmdList) {
			if (m_CommandList) {
				m_CommandList->BeginDebugEvent(name);
			}
		}
		
		~ScopedDebugEvent() {
			if (m_CommandList) {
				m_CommandList->EndDebugEvent();
			}
		}
		
		ScopedDebugEvent(const ScopedDebugEvent&) = delete;
		ScopedDebugEvent& operator=(const ScopedDebugEvent&) = delete;
		
	private:
		RHICommandList* m_CommandList;
	};

	#define RHI_SCOPED_EVENT(cmdList, name) \
		::Lunex::RHI::ScopedDebugEvent _debugEvent##__LINE__(cmdList, name)

	// ============================================================================
	// COMMAND QUEUE
	// ============================================================================
	
	/**
	 * @class RHICommandQueue
	 * @brief Queue for submitting command lists to the GPU
	 */
	class RHICommandQueue {
	public:
		virtual ~RHICommandQueue() = default;
		
		/**
		 * @brief Submit command lists for execution
		 * @param commandLists Array of command lists
		 * @param count Number of command lists
		 */
		virtual void Submit(RHICommandList* const* commandLists, uint32_t count) = 0;
		
		/**
		 * @brief Submit a single command list
		 */
		void Submit(RHICommandList* commandList) {
			Submit(&commandList, 1);
		}
		
		/**
		 * @brief Wait for all submitted work to complete
		 */
		virtual void WaitIdle() = 0;
		
		/**
		 * @brief Signal a fence after all current work completes
		 */
		virtual void Signal(class RHIFence* fence) = 0;
		
		// Factory
		static Ref<RHICommandQueue> CreateGraphics();
		static Ref<RHICommandQueue> CreateCompute();
		static Ref<RHICommandQueue> CreateCopy();
	};

	// ============================================================================
	// FENCE (GPU SYNCHRONIZATION)
	// ============================================================================
	
	/**
	 * @class RHIFence
	 * @brief GPU synchronization primitive
	 */
	class RHIFence : public RHIResource {
	public:
		virtual ~RHIFence() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Fence; }
		
		/**
		 * @brief Wait for fence to be signaled
		 * @param timeout Timeout in nanoseconds (0 = infinite)
		 * @return true if signaled, false if timeout
		 */
		virtual bool Wait(uint64_t timeout = 0) = 0;
		
		/**
		 * @brief Reset fence to unsignaled state
		 */
		virtual void Reset() = 0;
		
		/**
		 * @brief Check if fence is signaled without waiting
		 */
		virtual bool IsSignaled() const = 0;
		
		/**
		 * @brief Get current fence value (for timeline fences)
		 */
		virtual uint64_t GetValue() const = 0;
		
		// Factory
		static Ref<RHIFence> Create(bool signaled = false);
	};

} // namespace RHI
} // namespace Lunex
