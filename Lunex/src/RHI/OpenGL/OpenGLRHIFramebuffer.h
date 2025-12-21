#pragma once

/**
 * @file OpenGLRHIFramebuffer.h
 * @brief OpenGL implementation of RHI framebuffer
 */

#include "RHI/RHIFramebuffer.h"
#include <glad/glad.h>

namespace Lunex {
namespace RHI {

	class OpenGLRHIFramebuffer : public RHIFramebuffer {
	public:
		OpenGLRHIFramebuffer(const FramebufferDesc& desc);
		virtual ~OpenGLRHIFramebuffer();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_FramebufferID); }
		bool IsValid() const override { return m_FramebufferID != 0; }
		
		// Binding
		void Bind() override;
		void Unbind() override;
		void BindForRead() override;
		
		// Operations
		void Resize(uint32_t width, uint32_t height) override;
		void Clear(const ClearValue& colorValue, float depth = 1.0f, uint8_t stencil = 0) override;
		void ClearAttachment(uint32_t attachmentIndex, int value) override;
		void ClearDepth(float depth = 1.0f, uint8_t stencil = 0) override;
		
		// Texture access
		Ref<RHITexture2D> GetColorAttachment(uint32_t index = 0) const override;
		Ref<RHITexture2D> GetDepthAttachment() const override;
		RHIHandle GetColorAttachmentID(uint32_t index = 0) const override;
		RHIHandle GetDepthAttachmentID() const override;
		
		// Pixel reading
		int ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		void ReadPixels(uint32_t attachmentIndex, int x, int y, 
					   uint32_t width, uint32_t height, void* buffer) override;
		
		// Blit operations
		void BlitTo(RHIFramebuffer* dest, FilterMode filter = FilterMode::Linear) override;
		void BlitToScreen(uint32_t screenWidth, uint32_t screenHeight,
						 FilterMode filter = FilterMode::Linear) override;
		
		// OpenGL specific
		GLuint GetFramebufferID() const { return m_FramebufferID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		void CreateFramebuffer();
		void CreateAttachments();
		void AttachTexture(uint32_t index, Ref<RHITexture2D> texture);
		void AttachDepthTexture(Ref<RHITexture2D> texture);
		
		GLuint m_FramebufferID = 0;
		std::vector<Ref<RHITexture2D>> m_ColorAttachments;
		Ref<RHITexture2D> m_DepthAttachment;
	};

} // namespace RHI
} // namespace Lunex
