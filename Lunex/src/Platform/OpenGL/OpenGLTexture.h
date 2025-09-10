#pragma once

#include "Renderer/Texture.h"

namespace Lunex {
	
	class LUNEX_API OpenGLTexture2D : public Texture2D {
		public:
			OpenGLTexture2D(const std::string& path);
			virtual ~OpenGLTexture2D();
			
			virtual uint32_t GetWidth() const { return m_Width; }
			virtual uint32_t GetHeight() const { return m_Height; }
			
			virtual void Bind(uint32_t slot = 0) const;
	};
}