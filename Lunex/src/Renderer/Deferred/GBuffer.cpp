#include "stpch.h"
#include "GBuffer.h"
#include "Log/Log.h"

namespace Lunex {

	void GBuffer::Initialize(uint32_t width, uint32_t height) {
		m_Width = width;
		m_Height = height;

		FramebufferSpecification spec;
		spec.Width = width;
		spec.Height = height;
		spec.Attachments = {
			FramebufferTextureFormat::RGBA16F,      // RT0: Albedo.rgb + Metallic
			FramebufferTextureFormat::RGBA16F,      // RT1: Normal.xyz + Roughness
			FramebufferTextureFormat::RGBA16F,      // RT2: Emission.rgb + AO
			FramebufferTextureFormat::RGBA16F,      // RT3: Position.xyz + Specular
			FramebufferTextureFormat::RED_INTEGER,  // RT4: EntityID
			FramebufferTextureFormat::Depth         // Depth24Stencil8
		};

		m_Framebuffer = Framebuffer::Create(spec);
		LNX_LOG_INFO("GBuffer initialized: {0}x{1} ({2} color attachments + depth)", width, height, 
			static_cast<int>(GBufferAttachment::Count));
	}

	void GBuffer::Resize(uint32_t width, uint32_t height) {
		if (width == 0 || height == 0) return;
		if (width == m_Width && height == m_Height) return;

		m_Width = width;
		m_Height = height;

		if (m_Framebuffer) {
			m_Framebuffer->Resize(width, height);
		}
	}

	void GBuffer::Bind() {
		if (m_Framebuffer) {
			m_Framebuffer->Bind();
		}
	}

	void GBuffer::Unbind() {
		if (m_Framebuffer) {
			m_Framebuffer->Unbind();
		}
	}

	void GBuffer::Clear() {
		// Bind and clear is handled by the framebuffer + command list
		// The caller should set clear color and call clear via RHI
	}

	void GBuffer::ClearEntityID() {
		if (m_Framebuffer) {
			m_Framebuffer->ClearAttachment(
				static_cast<uint32_t>(GBufferAttachment::EntityID), -1);
		}
	}

	int GBuffer::ReadEntityID(int x, int y) {
		if (m_Framebuffer) {
			return m_Framebuffer->ReadPixel(
				static_cast<uint32_t>(GBufferAttachment::EntityID), x, y);
		}
		return -1;
	}

	uint32_t GBuffer::GetAttachmentRendererID(GBufferAttachment attachment) const {
		if (m_Framebuffer) {
			return m_Framebuffer->GetColorAttachmentRendererID(static_cast<uint32_t>(attachment));
		}
		return 0;
	}

	uint32_t GBuffer::GetDepthAttachmentRendererID() const {
		if (m_Framebuffer && m_Framebuffer->GetRHIFramebuffer()) {
			return static_cast<uint32_t>(m_Framebuffer->GetRHIFramebuffer()->GetDepthAttachmentID());
		}
		return 0;
	}

	uint32_t GBuffer::GetRendererID() const {
		if (m_Framebuffer) {
			return m_Framebuffer->GetRendererID();
		}
		return 0;
	}

} // namespace Lunex
