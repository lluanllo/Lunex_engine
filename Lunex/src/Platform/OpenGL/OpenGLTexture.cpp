#include "stpch.h"
#include "OpenGLTexture.h"

#include "stb_image.h"

#include <glad/glad.h>

namespace Lunex {
	OpenGLTexture2D::OpenGLTexture2D(const std::string& path) : m_Path(path) {
		
	}
	
	OpenGLTexture2D::~OpenGLTexture2D() {
		
	}
	
	void OpenGLTexture2D::Bind(uint32_t slot) const {
		
	}
}