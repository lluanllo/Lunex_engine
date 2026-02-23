/**
 * @file VulkanRHICommandList.cpp
 * @brief Vulkan implementation of RHI command list
 * 
 * Records commands into a VkCommandBuffer.
 * Unlike OpenGL's immediate mode, commands are truly deferred.
 */

#include "stpch.h"
#include "VulkanRHICommandList.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	VulkanRHICommandList::VulkanRHICommandList(VulkanRHIContext* context)
		: m_Context(context)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = context->GetCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkResult result = vkAllocateCommandBuffers(context->GetDevice(), &allocInfo, &m_CommandBuffer);
		if (result != VK_SUCCESS) {
			LNX_LOG_ERROR("Failed to allocate Vulkan command buffer!");
		}
	}

	VulkanRHICommandList::~VulkanRHICommandList() {
		if (m_CommandBuffer != VK_NULL_HANDLE && m_Context) {
			vkFreeCommandBuffers(m_Context->GetDevice(), m_Context->GetCommandPool(), 1, &m_CommandBuffer);
		}
	}

	// ============================================================================
	// LIFECYCLE
	// ============================================================================

	void VulkanRHICommandList::Begin() {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;

		vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
		m_Recording = true;
	}

	void VulkanRHICommandList::End() {
		if (m_InRenderPass) {
			vkCmdEndRenderPass(m_CommandBuffer);
			m_InRenderPass = false;
		}
		vkEndCommandBuffer(m_CommandBuffer);
		m_Recording = false;
	}

	void VulkanRHICommandList::Reset() {
		vkResetCommandBuffer(m_CommandBuffer, 0);
		m_Recording = false;
		m_InRenderPass = false;
	}

	// ============================================================================
	// IMMEDIATE STATE (compatibility layer)
	// ============================================================================

	void VulkanRHICommandList::SetClearColor(const glm::vec4& color) {
		m_ClearColor = color;
	}

	void VulkanRHICommandList::Clear() {
		// In Vulkan, clearing happens as part of render pass begin
		// This is a no-op; clear values are passed in BeginRenderPass
	}

	void VulkanRHICommandList::SetDepthMask(bool enabled) {
		// In Vulkan, this is part of pipeline state (DepthStencilState)
		// Dynamic state could be used with VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
	}

	void VulkanRHICommandList::SetDepthFunc(CompareFunc func) {
		m_CurrentDepthFunc = func;
		// In Vulkan, this is part of pipeline state or dynamic state
	}

	CompareFunc VulkanRHICommandList::GetDepthFunc() const {
		return m_CurrentDepthFunc;
	}

	void VulkanRHICommandList::SetLineWidth(float width) {
		if (m_Recording) {
			vkCmdSetLineWidth(m_CommandBuffer, width);
		}
	}

	void VulkanRHICommandList::DrawLines(uint32_t vertexCount, uint32_t firstVertex) {
		if (m_Recording) {
			vkCmdDraw(m_CommandBuffer, vertexCount, 1, firstVertex, 0);
		}
	}

	void VulkanRHICommandList::DrawArrays(uint32_t vertexCount, uint32_t firstVertex) {
		if (m_Recording) {
			vkCmdDraw(m_CommandBuffer, vertexCount, 1, firstVertex, 0);
		}
	}

	void VulkanRHICommandList::GetViewport(int* viewport) const {
		viewport[0] = static_cast<int>(m_CurrentViewport.X);
		viewport[1] = static_cast<int>(m_CurrentViewport.Y);
		viewport[2] = static_cast<int>(m_CurrentViewport.Width);
		viewport[3] = static_cast<int>(m_CurrentViewport.Height);
	}

	uint64_t VulkanRHICommandList::GetBoundFramebuffer() const {
		return 0; // Vulkan doesn't have a "bound framebuffer" in the OpenGL sense
	}

	void VulkanRHICommandList::SetDrawBuffers(const std::vector<uint32_t>& attachments) {
		// In Vulkan, this is defined by the render pass and subpass
	}

	// ============================================================================
	// RENDER STATE
	// ============================================================================

	void VulkanRHICommandList::SetDepthTestEnabled(bool enabled) {
		// Part of pipeline state in Vulkan
	}

	void VulkanRHICommandList::SetColorMask(bool r, bool g, bool b, bool a) {
		// Part of pipeline color blend state in Vulkan
	}

	void VulkanRHICommandList::SetPolygonOffset(bool enabled, float factor, float units) {
		if (m_Recording) {
			vkCmdSetDepthBias(m_CommandBuffer, units, 0.0f, factor);
		}
	}

	void VulkanRHICommandList::SetCullMode(CullMode mode) {
		// Part of pipeline rasterizer state; could use VK_DYNAMIC_STATE_CULL_MODE (Vulkan 1.3)
	}

	void VulkanRHICommandList::ClearDepthOnly(float depth) {
		// In Vulkan, handled via render pass clear values or vkCmdClearAttachments
		if (m_Recording && m_InRenderPass) {
			VkClearAttachment clearAttachment{};
			clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			clearAttachment.clearValue.depthStencil = { depth, 0 };

			VkClearRect clearRect{};
			clearRect.rect.offset = { 0, 0 };
			clearRect.rect.extent = { static_cast<uint32_t>(m_CurrentViewport.Width),
									  static_cast<uint32_t>(m_CurrentViewport.Height) };
			clearRect.baseArrayLayer = 0;
			clearRect.layerCount = 1;

			vkCmdClearAttachments(m_CommandBuffer, 1, &clearAttachment, 1, &clearRect);
		}
	}

	void VulkanRHICommandList::BindFramebufferByHandle(uint64_t handle) {
		// In Vulkan, framebuffer binding is part of BeginRenderPass
	}

	void VulkanRHICommandList::SetNoColorOutput() {
		// In Vulkan, this is defined by the render pass (depth-only subpass)
	}

	void VulkanRHICommandList::AttachDepthTextureLayer(uint64_t framebufferHandle, uint64_t textureHandle, uint32_t layer) {
		// In Vulkan, this is handled through VkImageView with specific layer when creating framebuffer
	}

	// ============================================================================
	// RENDER PASS
	// ============================================================================

	void VulkanRHICommandList::BeginRenderPass(const RenderPassBeginInfo& info) {
		// TODO: Implement with VkRenderPassBeginInfo
		// This requires a VkRenderPass and VkFramebuffer to be created first
		m_InRenderPass = true;
	}

	void VulkanRHICommandList::EndRenderPass() {
		if (m_InRenderPass) {
			vkCmdEndRenderPass(m_CommandBuffer);
			m_InRenderPass = false;
		}
	}

	// ============================================================================
	// PIPELINE
	// ============================================================================

	void VulkanRHICommandList::SetPipeline(const RHIGraphicsPipeline* pipeline) {
		// TODO: vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ...)
	}

	void VulkanRHICommandList::SetComputePipeline(const RHIComputePipeline* pipeline) {
		// TODO: vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ...)
	}

	// ============================================================================
	// VIEWPORT / SCISSOR
	// ============================================================================

	void VulkanRHICommandList::SetViewport(const Viewport& viewport) {
		m_CurrentViewport = viewport;
		if (m_Recording) {
			VkViewport vkViewport{};
			vkViewport.x = viewport.X;
			// Vulkan has Y-axis flipped compared to OpenGL
			vkViewport.y = viewport.Height + viewport.Y;
			vkViewport.width = viewport.Width;
			vkViewport.height = -viewport.Height;  // Negative for GL compatibility
			vkViewport.minDepth = viewport.MinDepth;
			vkViewport.maxDepth = viewport.MaxDepth;
			vkCmdSetViewport(m_CommandBuffer, 0, 1, &vkViewport);
		}
	}

	void VulkanRHICommandList::SetScissor(const ScissorRect& scissor) {
		if (m_Recording) {
			VkRect2D vkScissor{};
			vkScissor.offset = { scissor.X, scissor.Y };
			vkScissor.extent = { scissor.Width, scissor.Height };
			vkCmdSetScissor(m_CommandBuffer, 0, 1, &vkScissor);
		}
	}

	// ============================================================================
	// BUFFERS
	// ============================================================================

	void VulkanRHICommandList::SetVertexBuffer(const RHIBuffer* buffer, uint32_t slot, uint64_t offset) {
		if (!m_Recording || !buffer) return;
		VkBuffer vkBuffer = reinterpret_cast<VkBuffer>(buffer->GetNativeHandle());
		VkDeviceSize offsets[] = { offset };
		vkCmdBindVertexBuffers(m_CommandBuffer, slot, 1, &vkBuffer, offsets);
	}

	void VulkanRHICommandList::SetVertexBuffers(const RHIBuffer* const* buffers, uint32_t count, const uint64_t* offsets) {
		if (!m_Recording || !buffers || count == 0) return;

		std::vector<VkBuffer> vkBuffers(count);
		std::vector<VkDeviceSize> vkOffsets(count, 0);

		for (uint32_t i = 0; i < count; i++) {
			vkBuffers[i] = buffers[i] ? reinterpret_cast<VkBuffer>(buffers[i]->GetNativeHandle()) : VK_NULL_HANDLE;
			if (offsets) vkOffsets[i] = offsets[i];
		}

		vkCmdBindVertexBuffers(m_CommandBuffer, 0, count, vkBuffers.data(), vkOffsets.data());
	}

	void VulkanRHICommandList::SetIndexBuffer(const RHIBuffer* buffer, uint64_t offset) {
		if (!m_Recording || !buffer) return;
		VkBuffer vkBuffer = reinterpret_cast<VkBuffer>(buffer->GetNativeHandle());
		// TODO: Determine index type from buffer desc
		vkCmdBindIndexBuffer(m_CommandBuffer, vkBuffer, offset, VK_INDEX_TYPE_UINT32);
	}

	// ============================================================================
	// UNIFORMS
	// ============================================================================

	void VulkanRHICommandList::SetUniformBuffer(const RHIBuffer* buffer, uint32_t binding, ShaderStage stages) {
		// In Vulkan, uniforms are bound through descriptor sets
		// TODO: Implement descriptor set management
	}

	void VulkanRHICommandList::SetUniformBufferRange(const RHIBuffer* buffer, uint32_t binding,
		uint64_t offset, uint64_t size, ShaderStage stages) {
		// TODO: Dynamic uniform buffer offsets through descriptor sets
	}

	void VulkanRHICommandList::SetStorageBuffer(const RHIBuffer* buffer, uint32_t binding, ShaderStage stages) {
		// TODO: Storage buffer descriptor binding
	}

	// ============================================================================
	// TEXTURES
	// ============================================================================

	void VulkanRHICommandList::SetTexture(const RHITexture* texture, uint32_t slot) {
		// In Vulkan, textures are bound through descriptor sets
		// TODO: Implement descriptor set management
	}

	void VulkanRHICommandList::SetSampler(const RHISampler* sampler, uint32_t slot) {
		// TODO: Sampler descriptor binding
	}

	void VulkanRHICommandList::SetTextureAndSampler(const RHITexture* texture, const RHISampler* sampler, uint32_t slot) {
		// TODO: Combined image sampler descriptor
	}

	void VulkanRHICommandList::SetStorageTexture(const RHITexture* texture, uint32_t slot, BufferAccess access) {
		// TODO: Storage image descriptor
	}

	// ============================================================================
	// DRAW COMMANDS
	// ============================================================================

	void VulkanRHICommandList::DrawIndexed(const DrawArgs& args) {
		if (m_Recording) {
			vkCmdDrawIndexed(m_CommandBuffer, args.IndexCount, args.InstanceCount,
				args.FirstIndex, args.VertexOffset, args.FirstInstance);
		}
	}

	void VulkanRHICommandList::Draw(const DrawArrayArgs& args) {
		if (m_Recording) {
			vkCmdDraw(m_CommandBuffer, args.VertexCount, args.InstanceCount,
				args.FirstVertex, args.FirstInstance);
		}
	}

	void VulkanRHICommandList::DrawIndexedIndirect(const RHIBuffer* argsBuffer, uint64_t offset) {
		if (!m_Recording || !argsBuffer) return;
		VkBuffer vkBuffer = reinterpret_cast<VkBuffer>(argsBuffer->GetNativeHandle());
		vkCmdDrawIndexedIndirect(m_CommandBuffer, vkBuffer, offset, 1, sizeof(VkDrawIndexedIndirectCommand));
	}

	void VulkanRHICommandList::DrawIndexedIndirectCount(const RHIBuffer* argsBuffer,
		const RHIBuffer* countBuffer, uint64_t argsOffset, uint64_t countOffset, uint32_t maxDrawCount) {
		// Requires VK_KHR_draw_indirect_count or Vulkan 1.2
		// TODO: Implement when supported
	}

	// ============================================================================
	// COMPUTE
	// ============================================================================

	void VulkanRHICommandList::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
		if (m_Recording) {
			vkCmdDispatch(m_CommandBuffer, groupsX, groupsY, groupsZ);
		}
	}

	void VulkanRHICommandList::DispatchIndirect(const RHIBuffer* argsBuffer, uint64_t offset) {
		if (!m_Recording || !argsBuffer) return;
		VkBuffer vkBuffer = reinterpret_cast<VkBuffer>(argsBuffer->GetNativeHandle());
		vkCmdDispatchIndirect(m_CommandBuffer, vkBuffer, offset);
	}

	// ============================================================================
	// BARRIERS
	// ============================================================================

	void VulkanRHICommandList::ResourceBarriers(const RHI::ResourceBarrier* barriers, uint32_t count) {
		// TODO: Translate to VkImageMemoryBarrier / VkBufferMemoryBarrier
		// This is critical for proper Vulkan resource state management
	}

	void VulkanRHICommandList::MemoryBarrier() {
		if (m_Recording) {
			VkMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(m_CommandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 1, &barrier, 0, nullptr, 0, nullptr);
		}
	}

	// ============================================================================
	// COPY OPERATIONS
	// ============================================================================

	void VulkanRHICommandList::CopyBuffer(const RHIBuffer* src, RHIBuffer* dst,
		uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
		if (!m_Recording || !src || !dst) return;

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = srcOffset;
		copyRegion.dstOffset = dstOffset;
		copyRegion.size = size;

		VkBuffer vkSrc = reinterpret_cast<VkBuffer>(src->GetNativeHandle());
		VkBuffer vkDst = reinterpret_cast<VkBuffer>(dst->GetNativeHandle());
		vkCmdCopyBuffer(m_CommandBuffer, vkSrc, vkDst, 1, &copyRegion);
	}

	void VulkanRHICommandList::CopyTexture(const RHITexture* src, RHITexture* dst,
		const TextureRegion& srcRegion, const TextureRegion& dstRegion) {
		// TODO: Implement VkImageCopy
	}

	void VulkanRHICommandList::CopyBufferToTexture(const RHIBuffer* src, RHITexture* dst,
		uint64_t bufferOffset, const TextureRegion& textureRegion) {
		// TODO: Implement VkBufferImageCopy
	}

	void VulkanRHICommandList::CopyTextureToBuffer(const RHITexture* src, RHIBuffer* dst,
		const TextureRegion& textureRegion, uint64_t bufferOffset) {
		// TODO: Implement VkBufferImageCopy
	}

	// ============================================================================
	// CLEAR
	// ============================================================================

	void VulkanRHICommandList::ClearRenderTarget(RHITexture2D* texture, const ClearValue& value) {
		// TODO: Implement via vkCmdClearColorImage
	}

	void VulkanRHICommandList::ClearDepthStencil(RHITexture2D* texture, float depth, uint8_t stencil) {
		// TODO: Implement via vkCmdClearDepthStencilImage
	}

	// ============================================================================
	// DEBUG
	// ============================================================================

	void VulkanRHICommandList::BeginDebugEvent(const std::string& name) {
		// TODO: Use VK_EXT_debug_utils vkCmdBeginDebugUtilsLabelEXT
	}

	void VulkanRHICommandList::EndDebugEvent() {
		// TODO: Use VK_EXT_debug_utils vkCmdEndDebugUtilsLabelEXT
	}

	void VulkanRHICommandList::InsertDebugMarker(const std::string& name) {
		// TODO: Use VK_EXT_debug_utils vkCmdInsertDebugUtilsLabelEXT
	}

} // namespace RHI
} // namespace Lunex
