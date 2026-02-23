#include "stpch.h"
#include "OpenGLRHICommandList.h"
#include "OpenGLRHIBuffer.h"
#include "OpenGLRHITexture.h"
#include "OpenGLRHIShader.h"
#include "OpenGLRHIFramebuffer.h"
#include "OpenGLRHIDevice.h"
#include "RHI/RHI.h"
#include "RHI/RHICommandList.h"
#include "RHI/RHIContext.h"
#include "RHI/Vulkan/VulkanRHICommandList.h"
#include "RHI/Vulkan/VulkanRHIContext.h"
#include "Log/Log.h"

// Fallback defines for missing GLAD extensions
#ifndef GLAD_GL_KHR_debug
#define GLAD_GL_KHR_debug 0
#endif

#ifndef GLAD_GL_ARB_indirect_parameters
#define GLAD_GL_ARB_indirect_parameters 0
#endif

#ifndef GL_PARAMETER_BUFFER
#define GL_PARAMETER_BUFFER 0x80EE
#endif

namespace Lunex {
	namespace RHI {

		// Helper function to convert CompareFunc to GL
		static GLenum CompareFuncToGL(CompareFunc func) {
			switch (func) {
			case CompareFunc::Never:        return GL_NEVER;
			case CompareFunc::Less:         return GL_LESS;
			case CompareFunc::Equal:        return GL_EQUAL;
			case CompareFunc::LessEqual:    return GL_LEQUAL;
			case CompareFunc::Greater:      return GL_GREATER;
			case CompareFunc::NotEqual:     return GL_NOTEQUAL;
			case CompareFunc::GreaterEqual: return GL_GEQUAL;
			case CompareFunc::Always:       return GL_ALWAYS;
			}
			return GL_LESS;
		}

		OpenGLRHICommandList::OpenGLRHICommandList() {}
		OpenGLRHICommandList::~OpenGLRHICommandList() {}

		void OpenGLRHICommandList::Begin() { m_Recording = true; }
		void OpenGLRHICommandList::End() { m_Recording = false; }
		void OpenGLRHICommandList::Reset() { m_CurrentFramebuffer = nullptr; m_CurrentPipeline = nullptr; }

		// ============================================================================
		// IMMEDIATE STATE (for legacy compatibility)
		// ============================================================================

		void OpenGLRHICommandList::SetClearColor(const glm::vec4& color) {
			m_ClearColor = color;
			glClearColor(color.r, color.g, color.b, color.a);
		}

		void OpenGLRHICommandList::Clear() {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		void OpenGLRHICommandList::SetDepthMask(bool enabled) {
			glDepthMask(enabled ? GL_TRUE : GL_FALSE);
		}

		void OpenGLRHICommandList::SetDepthFunc(CompareFunc func) {
			m_CurrentDepthFunc = func;
			glDepthFunc(CompareFuncToGL(func));
		}

		CompareFunc OpenGLRHICommandList::GetDepthFunc() const {
			return m_CurrentDepthFunc;
		}

		void OpenGLRHICommandList::SetLineWidth(float width) {
			glLineWidth(width);
		}

		void OpenGLRHICommandList::DrawLines(uint32_t vertexCount, uint32_t firstVertex) {
			glDrawArrays(GL_LINES, firstVertex, vertexCount);

			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->GetMutableStatistics().DrawCalls++;
			}
		}

		void OpenGLRHICommandList::DrawArrays(uint32_t vertexCount, uint32_t firstVertex) {
			glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);

			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->GetMutableStatistics().DrawCalls++;
				device->GetMutableStatistics().TrianglesDrawn += vertexCount / 3;
			}
		}

		void OpenGLRHICommandList::GetViewport(int* viewport) const {
			glGetIntegerv(GL_VIEWPORT, viewport);
		}

		uint64_t OpenGLRHICommandList::GetBoundFramebuffer() const {
			GLint fbo = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
			return static_cast<uint64_t>(fbo);
		}

		void OpenGLRHICommandList::SetDrawBuffers(const std::vector<uint32_t>& attachments) {
			if (attachments.empty()) {
				glDrawBuffer(GL_NONE);
				return;
			}

			std::vector<GLenum> glAttachments;
			glAttachments.reserve(attachments.size());

			for (uint32_t attachment : attachments) {
				glAttachments.push_back(GL_COLOR_ATTACHMENT0 + attachment);
			}

			glDrawBuffers(static_cast<GLsizei>(glAttachments.size()), glAttachments.data());
		}

		void OpenGLRHICommandList::SetColorMask(bool r, bool g, bool b, bool a) {
			glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE,
				b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE);
		}

		void OpenGLRHICommandList::SetDepthTestEnabled(bool enabled) {
			if (enabled) {
				glEnable(GL_DEPTH_TEST);
			}
			else {
				glDisable(GL_DEPTH_TEST);
			}
		}

		void OpenGLRHICommandList::SetPolygonOffset(bool enabled, float factor, float units) {
			if (enabled) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(factor, units);
			}
			else {
				glDisable(GL_POLYGON_OFFSET_FILL);
			}
		}

		void OpenGLRHICommandList::SetCullMode(CullMode mode) {
			if (mode == CullMode::None) {
				glDisable(GL_CULL_FACE);
			}
			else {
				glEnable(GL_CULL_FACE);
				glCullFace(mode == CullMode::Front ? GL_FRONT : GL_BACK);
			}
		}

		void OpenGLRHICommandList::ClearDepthOnly(float depth) {
			glClearDepthf(depth);
			glClear(GL_DEPTH_BUFFER_BIT);
		}

		void OpenGLRHICommandList::BindFramebufferByHandle(uint64_t handle) {
			glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(handle));
		}

		void OpenGLRHICommandList::SetNoColorOutput() {
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		}

		void OpenGLRHICommandList::AttachDepthTextureLayer(uint64_t framebufferHandle, uint64_t textureHandle, uint32_t layer) {
			glNamedFramebufferTextureLayer(
				static_cast<GLuint>(framebufferHandle),
				GL_DEPTH_ATTACHMENT,
				static_cast<GLuint>(textureHandle),
				0, layer
			);
		}

		void OpenGLRHICommandList::BeginRenderPass(const RenderPassBeginInfo& info) {
			m_CurrentFramebuffer = info.Framebuffer;
			if (m_CurrentFramebuffer) {
				m_CurrentFramebuffer->Bind();

				if (info.ClearColor || info.ClearDepth) {
					ClearValue clearVal = info.ClearValues.empty() ? ClearValue::ColorValue(0, 0, 0, 1) : info.ClearValues[0];
					float depth = info.ClearDepth ? 1.0f : 0.0f;
					m_CurrentFramebuffer->Clear(clearVal, depth, 0);
				}

				SetViewport(info.RenderViewport);
				if (info.UseScissor) SetScissor(info.RenderScissor);
			}
		}

		void OpenGLRHICommandList::EndRenderPass() {
			if (m_CurrentFramebuffer) {
				m_CurrentFramebuffer->Unbind();
				m_CurrentFramebuffer = nullptr;
			}
		}

		void OpenGLRHICommandList::SetPipeline(const RHIGraphicsPipeline* pipeline) {
			m_CurrentPipeline = pipeline;
			if (pipeline) pipeline->Bind();
		}

		void OpenGLRHICommandList::SetComputePipeline(const RHIComputePipeline* pipeline) {
			m_CurrentComputePipeline = pipeline;
			if (pipeline) pipeline->Bind();
		}

		void OpenGLRHICommandList::SetViewport(const Viewport& viewport) {
			glViewport(static_cast<GLint>(viewport.X), static_cast<GLint>(viewport.Y),
				static_cast<GLsizei>(viewport.Width), static_cast<GLsizei>(viewport.Height));
			glDepthRange(viewport.MinDepth, viewport.MaxDepth);
		}

		void OpenGLRHICommandList::SetScissor(const ScissorRect& scissor) {
			glScissor(scissor.X, scissor.Y, scissor.Width, scissor.Height);
		}

		void OpenGLRHICommandList::SetVertexBuffer(const RHIBuffer* buffer, uint32_t slot, uint64_t offset) {
			if (auto* glBuf = dynamic_cast<const OpenGLRHIBuffer*>(buffer)) {
				glBuf->Bind();
			}
		}

		void OpenGLRHICommandList::SetVertexBuffers(const RHIBuffer* const* buffers, uint32_t count, const uint64_t* offsets) {
			for (uint32_t i = 0; i < count; i++) {
				SetVertexBuffer(buffers[i], i, offsets ? offsets[i] : 0);
			}
		}

		void OpenGLRHICommandList::SetIndexBuffer(const RHIBuffer* buffer, uint64_t offset) {
			if (auto* glBuf = dynamic_cast<const OpenGLRHIIndexBuffer*>(buffer)) {
				glBuf->Bind();
				m_CurrentIndexType = glBuf->GetIndexFormat();
			}
		}

		void OpenGLRHICommandList::SetUniformBuffer(const RHIBuffer* buffer, uint32_t binding, ShaderStage stages) {
			if (auto* glBuf = dynamic_cast<const OpenGLRHIBuffer*>(buffer)) {
				glBuf->BindToPoint(binding);
			}
		}

		void OpenGLRHICommandList::SetUniformBufferRange(const RHIBuffer* buffer, uint32_t binding,
			uint64_t offset, uint64_t size, ShaderStage stages) {
			if (auto* glBuf = dynamic_cast<const OpenGLRHIBuffer*>(buffer)) {
				glBindBufferRange(GL_UNIFORM_BUFFER, binding,
					static_cast<GLuint>(glBuf->GetNativeHandle()), offset, size);
			}
		}

		void OpenGLRHICommandList::SetStorageBuffer(const RHIBuffer* buffer, uint32_t binding, ShaderStage stages) {
			if (auto* glBuf = dynamic_cast<const OpenGLRHIBuffer*>(buffer)) {
				glBuf->BindToPoint(binding);
			}
		}

		void OpenGLRHICommandList::SetTexture(const RHITexture* texture, uint32_t slot) {
			if (texture) texture->Bind(slot);
		}

		void OpenGLRHICommandList::SetSampler(const RHISampler* sampler, uint32_t slot) {
			if (sampler) sampler->Bind(slot);
		}

		void OpenGLRHICommandList::SetTextureAndSampler(const RHITexture* texture, const RHISampler* sampler, uint32_t slot) {
			SetTexture(texture, slot);
			SetSampler(sampler, slot);
		}

		void OpenGLRHICommandList::SetStorageTexture(const RHITexture* texture, uint32_t slot, BufferAccess access) {
			if (texture) texture->BindAsImage(slot, access, 0);
		}

		void OpenGLRHICommandList::DrawIndexed(const DrawArgs& args) {
			GLenum indexType = m_CurrentIndexType == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			glDrawElementsInstancedBaseVertex(GL_TRIANGLES, args.IndexCount, indexType,
				(void*)(args.FirstIndex * GetIndexTypeSize(m_CurrentIndexType)),
				args.InstanceCount, args.VertexOffset);

			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->GetMutableStatistics().DrawCalls++;
				device->GetMutableStatistics().TrianglesDrawn += args.IndexCount / 3;
			}
		}

		void OpenGLRHICommandList::Draw(const DrawArrayArgs& args) {
			glDrawArraysInstanced(GL_TRIANGLES, args.FirstVertex, args.VertexCount, args.InstanceCount);

			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->GetMutableStatistics().DrawCalls++;
				device->GetMutableStatistics().TrianglesDrawn += args.VertexCount / 3;
			}
		}

		void OpenGLRHICommandList::DrawIndexedIndirect(const RHIBuffer* argsBuffer, uint64_t offset) {
			if (auto* glBuf = dynamic_cast<const OpenGLRHIBuffer*>(argsBuffer)) {
				glBuf->Bind();
				GLenum indexType = m_CurrentIndexType == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
				glDrawElementsIndirect(GL_TRIANGLES, indexType, (void*)offset);
			}
		}

		void OpenGLRHICommandList::DrawIndexedIndirectCount(const RHIBuffer* argsBuffer, const RHIBuffer* countBuffer,
			uint64_t argsOffset, uint64_t countOffset, uint32_t maxDrawCount) {
			// OpenGL 4.6+ feature
			if (GLAD_GL_ARB_indirect_parameters) {
				if (auto* glArgs = dynamic_cast<const OpenGLRHIBuffer*>(argsBuffer)) {
					if (auto* glCount = dynamic_cast<const OpenGLRHIBuffer*>(countBuffer)) {
						glBindBuffer(GL_PARAMETER_BUFFER, static_cast<GLuint>(glCount->GetNativeHandle()));
						GLenum indexType = m_CurrentIndexType == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
						glMultiDrawElementsIndirectCount(GL_TRIANGLES, indexType, (void*)argsOffset,
							countOffset, maxDrawCount, 0);
					}
				}
			}
		}

		void OpenGLRHICommandList::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
			glDispatchCompute(groupsX, groupsY, groupsZ);
		}

		void OpenGLRHICommandList::DispatchIndirect(const RHIBuffer* argsBuffer, uint64_t offset) {
			if (auto* glBuf = dynamic_cast<const OpenGLRHIBuffer*>(argsBuffer)) {
				glBuf->Bind();
				glDispatchComputeIndirect(static_cast<GLintptr>(offset));
			}
		}

		void OpenGLRHICommandList::ResourceBarriers(const RHI::ResourceBarrier* barriers, uint32_t count) {
			// OpenGL doesn't have explicit barriers like Vulkan, but we can use memory barriers
			GLbitfield barrierBits = 0;

			for (uint32_t i = 0; i < count; i++) {
				switch (barriers[i].StateAfter) {
				case ResourceState::ShaderResource:
					barrierBits |= GL_TEXTURE_FETCH_BARRIER_BIT;
					break;
				case ResourceState::UnorderedAccess:
					barrierBits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
					break;
				case ResourceState::RenderTarget:
					barrierBits |= GL_FRAMEBUFFER_BARRIER_BIT;
					break;
				default: break;
				}
			}

			if (barrierBits) glMemoryBarrier(barrierBits);
		}

		void OpenGLRHICommandList::MemoryBarrier() {
			glMemoryBarrier(GL_ALL_BARRIER_BITS);
		}

		void OpenGLRHICommandList::CopyBuffer(const RHIBuffer* src, RHIBuffer* dst,
			uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
			if (auto* glSrc = dynamic_cast<const OpenGLRHIBuffer*>(src)) {
				if (auto* glDst = dynamic_cast<OpenGLRHIBuffer*>(dst)) {
					glCopyNamedBufferSubData(static_cast<GLuint>(glSrc->GetNativeHandle()),
						static_cast<GLuint>(glDst->GetNativeHandle()),
						srcOffset, dstOffset, size);
				}
			}
		}

		void OpenGLRHICommandList::CopyTexture(const RHITexture* src, RHITexture* dst,
			const TextureRegion& srcRegion, const TextureRegion& dstRegion) {
			// TODO: Implement texture copy
		}

		void OpenGLRHICommandList::CopyBufferToTexture(const RHIBuffer* src, RHITexture* dst,
			uint64_t bufferOffset, const TextureRegion& textureRegion) {
			// TODO: Implement buffer to texture copy
		}

		void OpenGLRHICommandList::CopyTextureToBuffer(const RHITexture* src, RHIBuffer* dst,
			const TextureRegion& textureRegion, uint64_t bufferOffset) {
			// TODO: Implement texture to buffer copy
		}

		void OpenGLRHICommandList::ClearRenderTarget(RHITexture2D* texture, const ClearValue& value) {
			if (texture) texture->Clear(value);
		}

		void OpenGLRHICommandList::ClearDepthStencil(RHITexture2D* texture, float depth, uint8_t stencil) {
			ClearValue val = ClearValue::DepthValue(depth, stencil);
			if (texture) texture->Clear(val);
		}

		void OpenGLRHICommandList::BeginDebugEvent(const std::string& name) {
			if (GLAD_GL_KHR_debug) {
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str());
			}
		}

		void OpenGLRHICommandList::EndDebugEvent() {
			if (GLAD_GL_KHR_debug) {
				glPopDebugGroup();
			}
		}

		void OpenGLRHICommandList::InsertDebugMarker(const std::string& name) {
			if (GLAD_GL_KHR_debug) {
				glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER,
					0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, name.c_str());
			}
		}

		// ============================================================================
		// FACTORY IMPLEMENTATIONS
		// ============================================================================

		Ref<RHICommandList> RHICommandList::CreateGraphics() {
			switch (RHI::GetCurrentAPI()) {
			case GraphicsAPI::Vulkan: {
				auto* vulkanCtx = dynamic_cast<VulkanRHIContext*>(RHIContext::Get());
				if (vulkanCtx) {
					return CreateRef<VulkanRHICommandList>(vulkanCtx);
				}
				LNX_LOG_ERROR("RHICommandList::CreateGraphics - Vulkan context not available");
				return nullptr;
			}
			case GraphicsAPI::OpenGL:
			default:
				return CreateRef<OpenGLRHICommandList>();
			}
		}

		Ref<RHICommandList> RHICommandList::CreateCompute() {
			switch (RHI::GetCurrentAPI()) {
			case GraphicsAPI::Vulkan: {
				auto* vulkanCtx = dynamic_cast<VulkanRHIContext*>(RHIContext::Get());
				if (vulkanCtx) {
					return CreateRef<VulkanRHICommandList>(vulkanCtx);
				}
				LNX_LOG_ERROR("RHICommandList::CreateCompute - Vulkan context not available");
				return nullptr;
			}
			case GraphicsAPI::OpenGL:
			default:
				return CreateRef<OpenGLRHICommandList>();
			}
		}

		Ref<RHICommandList> RHICommandList::CreateCopy() {
			switch (RHI::GetCurrentAPI()) {
			case GraphicsAPI::Vulkan: {
				auto* vulkanCtx = dynamic_cast<VulkanRHIContext*>(RHIContext::Get());
				if (vulkanCtx) {
					return CreateRef<VulkanRHICommandList>(vulkanCtx);
				}
				LNX_LOG_ERROR("RHICommandList::CreateCopy - Vulkan context not available");
				return nullptr;
			}
			case GraphicsAPI::OpenGL:
			default:
				return CreateRef<OpenGLRHICommandList>();
			}
		}

	} // namespace RHI
} // namespace Lunex