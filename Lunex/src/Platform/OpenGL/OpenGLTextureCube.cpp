#include "stpch.h"
#include "OpenGLTextureCube.h"
#include "Log/Log.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Lunex {

	OpenGLTextureCube::OpenGLTextureCube(const std::string& path) {
		LoadFromSingleFile(path);
	}

	OpenGLTextureCube::OpenGLTextureCube(const std::array<std::string, 6>& faces) {
		LoadFromSixFiles(faces);
	}

	OpenGLTextureCube::~OpenGLTextureCube() {
		if (m_RendererID)
			glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTextureCube::LoadFromSingleFile(const std::string& path) {
		// Load equirectangular HDR or single cubemap file
		// For now, we'll skip HDR and just support traditional cubemaps
		LNX_LOG_WARN("Single file cubemap loading not yet implemented: {0}", path);
		m_IsLoaded = false;
	}

	void OpenGLTextureCube::LoadFromSixFiles(const std::array<std::string, 6>& faces) {
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

		stbi_set_flip_vertically_on_load(false); // Cubemaps don't need flipping

		int width, height, channels;
		for (unsigned int i = 0; i < faces.size(); i++) {
			unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &channels, 0);
			
			if (data) {
				GLenum format = GL_RGB;
				if (channels == 4) format = GL_RGBA;
				else if (channels == 3) format = GL_RGB;
				else if (channels == 1) format = GL_RED;

				glTexImage2D(
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
					0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data
				);
				
				stbi_image_free(data);
				
				if (i == 0) {
					m_Width = width;
					m_Height = height;
				}
				
				LNX_LOG_TRACE("Loaded cubemap face {0}: {1}", i, faces[i]);
			}
			else {
				LNX_LOG_ERROR("Cubemap texture failed to load at path: {0}", faces[i]);
				LNX_LOG_ERROR("STB Error: {0}", stbi_failure_reason());
				m_IsLoaded = false;
				return;
			}
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		m_IsLoaded = true;
		LNX_LOG_INFO("Cubemap texture loaded successfully");
	}

	void OpenGLTextureCube::Bind(uint32_t slot) const {
		glBindTextureUnit(slot, m_RendererID);
	}

}
