/**
 * @file VulkanRHICommandList.h
 * @brief Vulkan implementation of RHI command list (VkCommandBuffer)
 */

#pragma once

#include "RHI/RHICommandList.h"
#include "VulkanRHIContext.h"

namespace Lunex {
namespace RHI {

	class VulkanRHICommandList : public RHICommandList {
	public:
		VulkanRHICommandList(VulkanRHIContext* context);
		virtual ~VulkanRHICommandList();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_CommandBuffer); }
		bool IsValid() const override { return m_CommandBuffer != VK_NULL_HANDLE; }

		// Lifecycle
		void Begin() override;
		void End() override;
		void Reset() override;
		bool IsRecording() const override { return m_Recording; }

		// Immediate state
		void SetClearColor(const glm::vec4& color) override;
		void Clear() override;
		void SetDepthMask(bool enabled) override;
		void SetDepthFunc(CompareFunc func) override;
		CompareFunc GetDepthFunc() const override;
		void SetLineWidth(float width) override;
		void DrawLines(uint32_t vertexCount, uint32_t firstVertex = 0) override;
		void DrawArrays(uint32_t vertexCount, uint32_t firstVertex = 0) override;
		void GetViewport(int* viewport) const override;
		uint64_t GetBoundFramebuffer() const override;
		void SetDrawBuffers(const std::vector<uint32_t>& attachments) override;

		// Render state
		void SetDepthTestEnabled(bool enabled) override;
		void SetColorMask(bool r, bool g, bool b, bool a) override;
		void SetPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f) override;
		void SetCullMode(CullMode mode) override;
		void ClearDepthOnly(float depth = 1.0f) override;
		void BindFramebufferByHandle(uint64_t handle) override;
		void SetNoColorOutput() override;
		void AttachDepthTextureLayer(uint64_t framebufferHandle, uint64_t textureHandle, uint32_t layer) override;

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
		void ResourceBarriers(const RHI::ResourceBarrier* barriers, uint32_t count) override;
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

		// Vulkan specific
		VkCommandBuffer GetVkCommandBuffer() const { return m_CommandBuffer; }

	protected:
		void OnDebugNameChanged() override {}

	private:
		VulkanRHIContext* m_Context = nullptr;
		VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
		bool m_Recording = false;
		bool m_InRenderPass = false;

		// Tracked state
		CompareFunc m_CurrentDepthFunc = CompareFunc::Less;
		glm::vec4 m_ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		Viewport m_CurrentViewport{};
	};

} // namespace RHI
} // namespace Lunex
