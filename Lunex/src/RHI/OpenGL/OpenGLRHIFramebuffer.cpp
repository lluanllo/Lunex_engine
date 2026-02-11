#include "stpch.h"
#include "OpenGLRHIFramebuffer.h"
#include "OpenGLRHITexture.h"
#include "OpenGLRHIDevice.h"
#include "Log/Log.h"

// Fallback defines for missing GLAD extensions
#ifndef GLAD_GL_KHR_debug
#define GLAD_GL_KHR_debug 0
#endif

namespace Lunex {
namespace RHI {

	OpenGLRHIFramebuffer::OpenGLRHIFramebuffer(const FramebufferDesc& desc) {
		m_Desc = desc;
		CreateFramebuffer();
	}
	
	OpenGLRHIFramebuffer::~OpenGLRHIFramebuffer() {
		if (m_FramebufferID) {
			glDeleteFramebuffers(1, &m_FramebufferID);
		}
	}
	
	void OpenGLRHIFramebuffer::CreateFramebuffer() {
		glCreateFramebuffers(1, &m_FramebufferID);
		
		// Create or attach color attachments
		m_ColorAttachments.resize(m_Desc.ColorAttachments.size());
		
		for (size_t i = 0; i < m_Desc.ColorAttachments.size(); i++) {
			const auto& attachmentDesc = m_Desc.ColorAttachments[i];
			
			if (attachmentDesc.ExistingTexture) {
				m_ColorAttachments[i] = attachmentDesc.ExistingTexture;
			} else {
				TextureDesc texDesc;
				texDesc.Width = m_Desc.Width;
				texDesc.Height = m_Desc.Height;
				texDesc.Format = attachmentDesc.Format;
				texDesc.SampleCount = m_Desc.SampleCount;
				texDesc.IsRenderTarget = true;
				texDesc.DebugName = m_Desc.DebugName + "_Color" + std::to_string(i);
				
				m_ColorAttachments[i] = RHITexture2D::Create(texDesc);
			}
			
			AttachTexture(static_cast<uint32_t>(i), m_ColorAttachments[i]);
		}
		
		// Create or attach depth attachment
		if (m_Desc.HasDepth) {
			if (m_Desc.DepthAttachment.ExistingTexture) {
				m_DepthAttachment = m_Desc.DepthAttachment.ExistingTexture;
			} else {
				TextureDesc texDesc;
				texDesc.Width = m_Desc.Width;
				texDesc.Height = m_Desc.Height;
				texDesc.Format = m_Desc.DepthAttachment.Format;
				texDesc.SampleCount = m_Desc.SampleCount;
				texDesc.IsRenderTarget = true;
				texDesc.DebugName = m_Desc.DebugName + "_Depth";
				
				m_DepthAttachment = RHITexture2D::Create(texDesc);
			}
			
			AttachDepthTexture(m_DepthAttachment);
		}
		
		// Set draw buffers
		if (!m_ColorAttachments.empty()) {
			std::vector<GLenum> attachments;
			for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
				attachments.push_back(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i));
			}
			glNamedFramebufferDrawBuffers(m_FramebufferID, 
										  static_cast<GLsizei>(attachments.size()), 
										  attachments.data());
		} else {
			glNamedFramebufferDrawBuffer(m_FramebufferID, GL_NONE);
			glNamedFramebufferReadBuffer(m_FramebufferID, GL_NONE);
		}
		
		// Check framebuffer completeness
		GLenum status = glCheckNamedFramebufferStatus(m_FramebufferID, GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			LNX_LOG_ERROR("Framebuffer incomplete! Status: 0x{0:X}", status);
		}
	}
	
	void OpenGLRHIFramebuffer::AttachTexture(uint32_t index, Ref<RHITexture2D> texture) {
		if (!texture) return;
		
		auto* glTexture = dynamic_cast<OpenGLRHITexture2D*>(texture.get());
		if (!glTexture) return;
		
		glNamedFramebufferTexture(m_FramebufferID, GL_COLOR_ATTACHMENT0 + index, 
								  glTexture->GetTextureID(), 0);
	}
	
	void OpenGLRHIFramebuffer::AttachDepthTexture(Ref<RHITexture2D> texture) {
		if (!texture) return;
		
		auto* glTexture = dynamic_cast<OpenGLRHITexture2D*>(texture.get());
		if (!glTexture) return;
		
		GLenum attachment = GL_DEPTH_ATTACHMENT;
		if (texture->GetFormat() == TextureFormat::Depth24Stencil8 ||
			texture->GetFormat() == TextureFormat::Depth32FStencil8) {
			attachment = GL_DEPTH_STENCIL_ATTACHMENT;
		}
		
		glNamedFramebufferTexture(m_FramebufferID, attachment, glTexture->GetTextureID(), 0);
	}
	
	void OpenGLRHIFramebuffer::Bind() {
		glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);
	}
	
	void OpenGLRHIFramebuffer::Unbind() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	
	void OpenGLRHIFramebuffer::BindForRead() {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FramebufferID);
	}
	
	void OpenGLRHIFramebuffer::Resize(uint32_t width, uint32_t height) {
		if (width == m_Desc.Width && height == m_Desc.Height) return;
		
		m_Desc.Width = width;
		m_Desc.Height = height;
		
		// Resize color attachments and re-attach to FBO
		// (Resize creates new GL texture IDs, so we must re-attach)
		for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
			if (m_ColorAttachments[i]) {
				m_ColorAttachments[i]->Resize(width, height);
				AttachTexture(static_cast<uint32_t>(i), m_ColorAttachments[i]);
			}
		}
		
		// Resize depth attachment and re-attach to FBO
		if (m_DepthAttachment) {
			m_DepthAttachment->Resize(width, height);
			AttachDepthTexture(m_DepthAttachment);
		}
	}
	
	void OpenGLRHIFramebuffer::Clear(const ClearValue& colorValue, float depth, uint8_t stencil) {
		Bind();
		
		// Clear color attachments
		for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
			glClearNamedFramebufferfv(m_FramebufferID, GL_COLOR, static_cast<GLint>(i), colorValue.Color);
		}
		
		// Clear depth/stencil
		if (m_DepthAttachment) {
			if (m_DepthAttachment->GetFormat() == TextureFormat::Depth24Stencil8 ||
				m_DepthAttachment->GetFormat() == TextureFormat::Depth32FStencil8) {
				glClearNamedFramebufferfi(m_FramebufferID, GL_DEPTH_STENCIL, 0, depth, stencil);
			} else {
				glClearNamedFramebufferfv(m_FramebufferID, GL_DEPTH, 0, &depth);
			}
		}
	}
	
	void OpenGLRHIFramebuffer::ClearAttachment(uint32_t attachmentIndex, int value) {
		if (attachmentIndex >= m_ColorAttachments.size()) return;
		
		glClearNamedFramebufferiv(m_FramebufferID, GL_COLOR, attachmentIndex, &value);
	}
	
	void OpenGLRHIFramebuffer::ClearDepth(float depth, uint8_t stencil) {
		if (!m_DepthAttachment) return;
		
		if (m_DepthAttachment->GetFormat() == TextureFormat::Depth24Stencil8 ||
			m_DepthAttachment->GetFormat() == TextureFormat::Depth32FStencil8) {
			glClearNamedFramebufferfi(m_FramebufferID, GL_DEPTH_STENCIL, 0, depth, stencil);
		} else {
			glClearNamedFramebufferfv(m_FramebufferID, GL_DEPTH, 0, &depth);
		}
	}
	
	Ref<RHITexture2D> OpenGLRHIFramebuffer::GetColorAttachment(uint32_t index) const {
		if (index >= m_ColorAttachments.size()) return nullptr;
		return m_ColorAttachments[index];
	}
	
	Ref<RHITexture2D> OpenGLRHIFramebuffer::GetDepthAttachment() const {
		return m_DepthAttachment;
	}
	
	RHIHandle OpenGLRHIFramebuffer::GetColorAttachmentID(uint32_t index) const {
		if (index >= m_ColorAttachments.size()) return 0;
		return m_ColorAttachments[index] ? m_ColorAttachments[index]->GetNativeHandle() : 0;
	}
	
	RHIHandle OpenGLRHIFramebuffer::GetDepthAttachmentID() const {
		return m_DepthAttachment ? m_DepthAttachment->GetNativeHandle() : 0;
	}
	
	int OpenGLRHIFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) {
		if (attachmentIndex >= m_ColorAttachments.size()) return 0;
		
		Bind();
		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
		
		int pixelData;
		glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
		
		return pixelData;
	}
	
	void OpenGLRHIFramebuffer::ReadPixels(uint32_t attachmentIndex, int x, int y, 
										  uint32_t width, uint32_t height, void* buffer) {
		if (attachmentIndex >= m_ColorAttachments.size()) return;
		
		Bind();
		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
		glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	}
	
	void OpenGLRHIFramebuffer::BlitTo(RHIFramebuffer* dest, FilterMode filter) {
		auto* glDest = dynamic_cast<OpenGLRHIFramebuffer*>(dest);
		if (!glDest) return;
		
		GLenum glFilter = filter == FilterMode::Linear ? GL_LINEAR : GL_NEAREST;
		
		glBlitNamedFramebuffer(
			m_FramebufferID, glDest->GetFramebufferID(),
			0, 0, m_Desc.Width, m_Desc.Height,
			0, 0, dest->GetWidth(), dest->GetHeight(),
			GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
			glFilter
		);
	}
	
	void OpenGLRHIFramebuffer::BlitToScreen(uint32_t screenWidth, uint32_t screenHeight, FilterMode filter) {
		GLenum glFilter = filter == FilterMode::Linear ? GL_LINEAR : GL_NEAREST;
		
		glBlitNamedFramebuffer(
			m_FramebufferID, 0,
			0, 0, m_Desc.Width, m_Desc.Height,
			0, 0, screenWidth, screenHeight,
			GL_COLOR_BUFFER_BIT,
			glFilter
		);
	}
	
	void OpenGLRHIFramebuffer::OnDebugNameChanged() {
		if (m_FramebufferID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_FRAMEBUFFER, m_FramebufferID, -1, m_DebugName.c_str());
		}
	}
	
	uint64_t RHIFramebuffer::GetGPUMemorySize() const {
		uint64_t size = 0;
		
		// Sum up all color attachments
		for (const auto& desc : m_Desc.ColorAttachments) {
			if (desc.ExistingTexture) {
				size += desc.ExistingTexture->GetGPUMemorySize();
			}
		}
		
		// Add depth attachment
		if (m_Desc.HasDepth && m_Desc.DepthAttachment.ExistingTexture) {
			size += m_Desc.DepthAttachment.ExistingTexture->GetGPUMemorySize();
		}
		
		return size;
	}
	
	// Factory
	Ref<RHIFramebuffer> RHIFramebuffer::Create(const FramebufferDesc& desc) {
		return CreateRef<OpenGLRHIFramebuffer>(desc);
	}
	
	Ref<RHIFramebuffer> RHIFramebuffer::Create(uint32_t width, uint32_t height, 
											   TextureFormat colorFormat, bool withDepth) {
		FramebufferDesc desc;
		desc.Width = width;
		desc.Height = height;
		desc.AddColorAttachment(colorFormat);
		
		if (withDepth) {
			desc.SetDepthAttachment();
		}
		
		return Create(desc);
	}

} // namespace RHI
} // namespace Lunex
