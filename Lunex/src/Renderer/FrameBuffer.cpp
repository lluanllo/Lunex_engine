#include "stpch.h"
#include "FrameBuffer.h"
#include "RHI/RHIDevice.h"
#include "Log/Log.h"

namespace Lunex {
	
	static const uint32_t s_MaxFramebufferSize = 8192;
	
	static bool IsDepthFormat(FramebufferTextureFormat format) {
		return format == FramebufferTextureFormat::DEPTH24STENCIL8;
	}
	
	Framebuffer::Framebuffer(const FramebufferSpecification& spec)
		: m_Specification(spec)
	{
		// Separate color and depth attachments
		for (const auto& attachment : m_Specification.Attachments.Attachments) {
			if (!IsDepthFormat(attachment.TextureFormat)) {
				m_ColorAttachmentSpecs.push_back(attachment);
			} else {
				m_DepthAttachmentSpec = attachment;
			}
		}
		
		Invalidate();
	}
	
	RHI::TextureFormat Framebuffer::ConvertFormat(FramebufferTextureFormat format) {
		switch (format) {
			case FramebufferTextureFormat::RGBA8:         return RHI::TextureFormat::RGBA8;
			case FramebufferTextureFormat::RED_INTEGER:   return RHI::TextureFormat::R32I;
			case FramebufferTextureFormat::DEPTH24STENCIL8: return RHI::TextureFormat::Depth24Stencil8;
			default: return RHI::TextureFormat::None;
		}
	}
	
	void Framebuffer::Invalidate() {
		RHI::FramebufferDesc desc;
		desc.Width = m_Specification.Width;
		desc.Height = m_Specification.Height;
		desc.SampleCount = m_Specification.Samples;
		
		// Add color attachments
		for (const auto& colorSpec : m_ColorAttachmentSpecs) {
			RHI::RenderTargetDesc rtDesc;
			rtDesc.Width = m_Specification.Width;
			rtDesc.Height = m_Specification.Height;
			rtDesc.Format = ConvertFormat(colorSpec.TextureFormat);
			rtDesc.SampleCount = m_Specification.Samples;
			desc.ColorAttachments.push_back(rtDesc);
		}
		
		// Add depth attachment if present
		if (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None) {
			desc.DepthAttachment.Width = m_Specification.Width;
			desc.DepthAttachment.Height = m_Specification.Height;
			desc.DepthAttachment.Format = ConvertFormat(m_DepthAttachmentSpec.TextureFormat);
			desc.DepthAttachment.SampleCount = m_Specification.Samples;
			desc.HasDepth = true;
		}
		
		m_RHIFramebuffer = RHI::RHIFramebuffer::Create(desc);
	}
	
	void Framebuffer::Bind() {
		if (m_RHIFramebuffer) {
			m_RHIFramebuffer->Bind();
		}
	}
	
	void Framebuffer::Unbind() {
		if (m_RHIFramebuffer) {
			m_RHIFramebuffer->Unbind();
		}
	}
	
	void Framebuffer::Resize(uint32_t width, uint32_t height) {
		if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize) {
			LNX_LOG_WARN("Attempted to resize framebuffer to {0}, {1}", width, height);
			return;
		}
		
		m_Specification.Width = width;
		m_Specification.Height = height;
		
		Invalidate();
	}
	
	int Framebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) {
		if (m_RHIFramebuffer) {
			return m_RHIFramebuffer->ReadPixel(attachmentIndex, x, y);
		}
		return -1;
	}
	
	void Framebuffer::ClearAttachment(uint32_t attachmentIndex, int value) {
		if (m_RHIFramebuffer) {
			m_RHIFramebuffer->ClearAttachment(attachmentIndex, value);
		}
	}
	
	uint32_t Framebuffer::GetColorAttachmentRendererID(uint32_t index) const {
		if (m_RHIFramebuffer) {
			return static_cast<uint32_t>(m_RHIFramebuffer->GetColorAttachmentID(index));
		}
		return 0;
	}
	
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec) {
		return CreateRef<Framebuffer>(spec);
	}
}