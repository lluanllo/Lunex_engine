#include "stpch.h"
#include "OpenGLTextureCube.h"
#include "Log/Log.h"
#include "Renderer/Shader.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Lunex {

	// ============================================================================
	// HELPER FUNCTIONS
	// ============================================================================

	// Helper function to render a cube (for conversion)
	static void RenderCube() {
		static uint32_t cubeVAO = 0;
		static uint32_t cubeVBO = 0;

		if (cubeVAO == 0) {
			float vertices[] = {
				// positions
				-1.0f,  1.0f, -1.0f,
				-1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,

				-1.0f, -1.0f,  1.0f,
				-1.0f, -1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f,  1.0f,
				-1.0f, -1.0f,  1.0f,

				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,

				-1.0f, -1.0f,  1.0f,
				-1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f, -1.0f,  1.0f,
				-1.0f, -1.0f,  1.0f,

				-1.0f,  1.0f, -1.0f,
				 1.0f,  1.0f, -1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				-1.0f,  1.0f,  1.0f,
				-1.0f,  1.0f, -1.0f,

				-1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f,  1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f,  1.0f,
				 1.0f, -1.0f,  1.0f
			};

			glGenVertexArrays(1, &cubeVAO);
			glGenBuffers(1, &cubeVBO);
			glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			glBindVertexArray(cubeVAO);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		}

		glBindVertexArray(cubeVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
	}

	// ============================================================================
	// OPENGLTEXTURECUBE IMPLEMENTATION
	// ============================================================================

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

	// ============================================================================
	// FACTORY METHODS
	// ============================================================================

	Ref<OpenGLTextureCube> OpenGLTextureCube::CreateFromHDR(const std::string& path) {
		Ref<OpenGLTextureCube> cubemap = CreateRef<OpenGLTextureCube>();
		cubemap->LoadFromHDREquirectangular(path);
		return cubemap;
	}

	Ref<OpenGLTextureCube> OpenGLTextureCube::CreateFromFaces(const std::array<std::string, 6>& faces) {
		return CreateRef<OpenGLTextureCube>(faces);
	}

	// ============================================================================
	// LOADING METHODS
	// ============================================================================

	void OpenGLTextureCube::LoadFromSingleFile(const std::string& path) {
		// Check if it's an HDR file
		if (path.ends_with(".hdr") || path.ends_with(".HDR")) {
			LoadFromHDREquirectangular(path);
		}
		else {
			LNX_LOG_WARN("Single file cubemap loading only supports HDR format: {0}", path);
			m_IsLoaded = false;
		}
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
		m_IsHDR = false;
		LNX_LOG_INFO("Cubemap texture loaded successfully");
	}

	void OpenGLTextureCube::LoadFromHDREquirectangular(const std::string& path) {
		LNX_LOG_INFO("Loading HDR equirectangular map: {0}", path);

		// Load HDR image using stb_image
		stbi_set_flip_vertically_on_load(true);
		int width, height, channels;
		float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);

		if (!data) {
			LNX_LOG_ERROR("Failed to load HDR image: {0}", path);
			LNX_LOG_ERROR("STB Error: {0}", stbi_failure_reason());
			m_IsLoaded = false;
			return;
		}

		LNX_LOG_INFO("HDR loaded: {0}x{1}, {2} channels", width, height, channels);

		// Create temporary equirectangular texture
		uint32_t hdrTexture;
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);

		// Convert equirectangular to cubemap
		uint32_t cubemapResolution = 512; // Adjust as needed
		ConvertEquirectangularToCubemap(hdrTexture, cubemapResolution);

		// Clean up temporary texture
		glDeleteTextures(1, &hdrTexture);

		m_Width = cubemapResolution;
		m_Height = cubemapResolution;
		m_IsLoaded = true;
		m_IsHDR = true;

		LNX_LOG_INFO("HDR cubemap created successfully ({0}x{0})", cubemapResolution);
	}

	void OpenGLTextureCube::ConvertEquirectangularToCubemap(uint32_t equirectangularMap, uint32_t resolution) {
		// Save current viewport
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		
		// Save current framebuffer
		GLint prevFBO;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
		
		// Create cubemap
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

		for (unsigned int i = 0; i < 6; ++i) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
				resolution, resolution, 0, GL_RGB, GL_FLOAT, nullptr);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Load conversion shader
		Ref<Shader> equirectToCubemapShader = Shader::Create("assets/shaders/EquirectToCubemap.glsl");

		// Setup framebuffer and renderbuffer
		uint32_t captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);

		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

		// Projection and view matrices for each cubemap face
		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] = {
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		// Create UBO for camera matrices
		struct CameraMatrices {
			glm::mat4 Projection;
			glm::mat4 View;
		};
		
		uint32_t cameraUBO;
		glGenBuffers(1, &cameraUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraMatrices), nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO);

		// Set projection once
		CameraMatrices matrices;
		matrices.Projection = captureProjection;

		// Bind shader and equirectangular map
		equirectToCubemapShader->Bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, equirectangularMap);

		glViewport(0, 0, resolution, resolution);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

		// Render cube for each face
		for (unsigned int i = 0; i < 6; ++i) {
			matrices.View = captureViews[i];
			
			// Update UBO with current view matrix
			glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraMatrices), &matrices);
			
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_RendererID, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			RenderCube();
		}

		// Restore previous state
		glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

		// Generate mipmaps
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

		// Cleanup
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
		glDeleteBuffers(1, &cameraUBO);
	}

	void OpenGLTextureCube::Bind(uint32_t slot) const {
		glBindTextureUnit(slot, m_RendererID);
	}

}
