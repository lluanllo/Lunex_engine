#pragma once

#include "Core/Core.h"
#include "RHI/RHIFramebuffer.h"
#include <vector>

namespace Lunex {
	
	enum class FramebufferTextureFormat {
		None = 0,
		
		// Color
		RGBA8,
		RGBA16F,
		RGBA32F,
		RG16F,
		RED_INTEGER,
		
		// Depth/stencil
		DEPTH24STENCIL8,
		
		// Defaults
		Depth = DEPTH24STENCIL8
	};
	
	struct FramebufferTextureSpecification {
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format)
			: TextureFormat(format) {
		}
		
		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
	};
	
	struct FramebufferAttachmentSpecification {
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
			: Attachments(attachments) {
		}
		
		std::vector<FramebufferTextureSpecification> Attachments;
	};
	
	struct FramebufferSpecification {
		uint32_t Width = 0, Height = 0;
		FramebufferAttachmentSpecification Attachments;
		uint32_t Samples = 1;
		
		bool SwapChainTarget = false;
	};
	
	/**
	 * @class Framebuffer
	 * @brief Framebuffer that uses RHI internally
	 */
	class Framebuffer {
	public:
		Framebuffer(const FramebufferSpecification& spec);
		~Framebuffer() = default;
		
		void Bind();
		void Unbind();
		
		void Resize(uint32_t width, uint32_t height);
		int ReadPixel(uint32_t attachmentIndex, int x, int y);
		
		void ClearAttachment(uint32_t attachmentIndex, int value);
		
		uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const;
		
		/**
		 * @brief Get the native framebuffer handle (e.g., OpenGL FBO ID)
		 */
		uint32_t GetRendererID() const;
		
		const FramebufferSpecification& GetSpecification() const { return m_Specification; }
		
		RHI::RHIFramebuffer* GetRHIFramebuffer() const { return m_RHIFramebuffer.get(); }
		
		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
		
	private:
		void Invalidate();
		static RHI::TextureFormat ConvertFormat(FramebufferTextureFormat format);
		
		FramebufferSpecification m_Specification;
		Ref<RHI::RHIFramebuffer> m_RHIFramebuffer;
		
		std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecs;
		FramebufferTextureSpecification m_DepthAttachmentSpec;
	};
}