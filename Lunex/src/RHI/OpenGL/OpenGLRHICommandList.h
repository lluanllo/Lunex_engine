#pragma once

/**
 * @file OpenGLRHICommandList.h
 * @brief OpenGL implementation of RHI command list (immediate mode)
 */

#include "RHI/RHICommandList.h"
#include "RHI/RHITypes.h"  // ADD: Ensure all types are visible
#include <glad/glad.h>

namespace Lunex {
namespace RHI {

	// Forward declare if needed
	struct ResourceBarrier;

	class OpenGLRHICommandList : public RHICommandList {
	public:
		OpenGLRHICommandList();
		virtual ~OpenGLRHICommandList();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return 0; }
		bool IsValid() const override { return true; }
		
		// Lifecycle
		void Begin() override;
		void End() override;
		void Reset() override;
		bool IsRecording() const override { return m_Recording; }
		
		// Render pass
		void BeginRenderPass(const RenderPassBeginInfo& info) override;
		void EndRenderPass() override;
		
		// Pipeline
		void SetPipeline(const RHIGraphicsPipeline* pipeline) override;
		void SetComputePipeline(const RHIComputePipeline* pipeline) override;
		
		// Viewport/Scissor
		void SetViewport(const Viewport& viewport) override;
		void SetScissor(const ScissorRect& scissor) override;
		
		// Buffers
		void SetVertexBuffer(const RHIBuffer* buffer, uint32_t slot, uint64_t offset) override;
		void SetVertexBuffers(const RHIBuffer* const* buffers, uint32_t count, const uint64_t* offsets) override;
		void SetIndexBuffer(const RHIBuffer* buffer, uint64_t offset) override;
		
		// Uniforms
		void SetUniformBuffer(const RHIBuffer* buffer, uint32_t binding, ShaderStage stages) override;
		void SetUniformBufferRange(const RHIBuffer* buffer, uint32_t binding, 
								  uint64_t offset, uint64_t size, ShaderStage stages) override;
		void SetStorageBuffer(const RHIBuffer* buffer, uint32_t binding, ShaderStage stages) override;
		
		// Textures
		void SetTexture(const RHITexture* texture, uint32_t slot) override;
		void SetSampler(const RHISampler* sampler, uint32_t slot) override;
		void SetTextureAndSampler(const RHITexture* texture, const RHISampler* sampler, uint32_t slot) override;
		void SetStorageTexture(const RHITexture* texture, uint32_t slot, BufferAccess access) override;
		
		// Draw commands
		void DrawIndexed(const DrawArgs& args) override;
		void Draw(const DrawArrayArgs& args) override;
		void DrawIndexedIndirect(const RHIBuffer* argsBuffer, uint64_t offset) override;
		void DrawIndexedIndirectCount(const RHIBuffer* argsBuffer, const RHIBuffer* countBuffer,
									  uint64_t argsOffset, uint64_t countOffset, uint32_t maxDrawCount) override;
		
		// Compute
		void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
		void DispatchIndirect(const RHIBuffer* argsBuffer, uint64_t offset) override;
		
		// Barriers
		void ResourceBarriers(const RHI::ResourceBarrier* barriers, uint32_t count) override;  // ADD: fully qualify type
		void MemoryBarrier() override;
		
		// Copy operations
		void CopyBuffer(const RHIBuffer* src, RHIBuffer* dst, 
					   uint64_t srcOffset, uint64_t dstOffset, uint64_t size) override;
		void CopyTexture(const RHITexture* src, RHITexture* dst,
						const TextureRegion& srcRegion, const TextureRegion& dstRegion) override;
		void CopyBufferToTexture(const RHIBuffer* src, RHITexture* dst,
								uint64_t bufferOffset, const TextureRegion& textureRegion) override;
		void CopyTextureToBuffer(const RHITexture* src, RHIBuffer* dst,
								const TextureRegion& textureRegion, uint64_t bufferOffset) override;
		
		// Clear
		void ClearRenderTarget(RHITexture2D* texture, const ClearValue& value) override;
		void ClearDepthStencil(RHITexture2D* texture, float depth, uint8_t stencil) override;
		
		// Debug
		void BeginDebugEvent(const std::string& name) override;
		void EndDebugEvent() override;
		void InsertDebugMarker(const std::string& name) override;
		
	protected:
		void OnDebugNameChanged() override {}
		
	private:
		bool m_Recording = false;
		RHIFramebuffer* m_CurrentFramebuffer = nullptr;
		const RHIGraphicsPipeline* m_CurrentPipeline = nullptr;
		const RHIComputePipeline* m_CurrentComputePipeline = nullptr;
		IndexType m_CurrentIndexType = IndexType::UInt32;
	};

} // namespace RHI
} // namespace Lunex
