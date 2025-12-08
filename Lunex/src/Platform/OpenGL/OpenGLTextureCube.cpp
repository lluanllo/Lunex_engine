#include "stpch.h"
#include "OpenGLTextureCube.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Renderer/FrameBuffer.h"

#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {
	
	// ========================================
	// CONSTRUCTOR - FROM 6 FACE IMAGES
	// ========================================
	
	OpenGLTextureCube::OpenGLTextureCube(const std::array<std::string, 6>& facePaths) {
		LNX_PROFILE_FUNCTION();
		LoadFaces(facePaths);
	}
	
	// ========================================
	// CONSTRUCTOR - EMPTY CUBEMAP
	// ========================================
	
	OpenGLTextureCube::OpenGLTextureCube(uint32_t size, bool hdr, uint32_t mipLevels) {
		LNX_PROFILE_FUNCTION();
		CreateEmpty(size, hdr, mipLevels);
	}
	
	// ========================================
	// DESTRUCTOR
	// ========================================
	
	OpenGLTextureCube::~OpenGLTextureCube() {
		LNX_PROFILE_FUNCTION();
		if (m_RendererID) {
			glDeleteTextures(1, &m_RendererID);
		}
	}
	
	// ========================================
	// BINDING
	// ========================================
	
	void OpenGLTextureCube::Bind(uint32_t slot) const {
		LNX_PROFILE_FUNCTION();
		glBindTextureUnit(slot, m_RendererID);
	}
	
	void OpenGLTextureCube::Unbind() const {
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
	
	// ========================================
	// LOAD 6 FACE IMAGES
	// ========================================
	
	void OpenGLTextureCube::LoadFaces(const std::array<std::string, 6>& facePaths) {
		m_IsLoaded = false;
		
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		
		int width, height, channels;
		stbi_set_flip_vertically_on_load(false); // Cubemaps don't need vertical flip
		
		for (uint32_t i = 0; i < 6; i++) {
			// Try HDR first
			float* hdrData = stbi_loadf(facePaths[i].c_str(), &width, &height, &channels, 0);
			
			if (hdrData) {
				m_IsHDR = true;
				
				if (i == 0) {
					m_Width = width;
					m_Height = height;
				}
				
				GLenum internalFormat = channels == 4 ? GL_RGBA16F : GL_RGB16F;
				GLenum dataFormat = channels == 4 ? GL_RGBA : GL_RGB;
				
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
					width, height, 0, dataFormat, GL_FLOAT, hdrData);
				
				stbi_image_free(hdrData);
				LNX_LOG_TRACE("Loaded HDR cubemap face {0}: {1}x{2}", i, width, height);
			}
			else {
				// Fallback to LDR
				unsigned char* ldrData = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 0);
				
				if (ldrData) {
					m_IsHDR = false;
					
					if (i == 0) {
						m_Width = width;
						m_Height = height;
					}
					
					GLenum internalFormat = channels == 4 ? GL_RGBA8 : GL_RGB8;
					GLenum dataFormat = channels == 4 ? GL_RGBA : GL_RGB;
					
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
						width, height, 0, dataFormat, GL_UNSIGNED_BYTE, ldrData);
					
					stbi_image_free(ldrData);
					LNX_LOG_TRACE("Loaded LDR cubemap face {0}: {1}x{2}", i, width, height);
				}
				else {
					LNX_LOG_ERROR("Failed to load cubemap face {0}: {1}", i, facePaths[i]);
					LNX_LOG_ERROR("  Reason: {0}", stbi_failure_reason());
					glDeleteTextures(1, &m_RendererID);
					m_RendererID = 0;
					return;
				}
			}
		}
		
		// Set texture parameters
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		
		// Enable seamless cubemap filtering
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		
		// Generate mipmaps
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_Width, m_Height)))) + 1;
		
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		m_IsLoaded = true;
		
		LNX_LOG_INFO("Cubemap loaded successfully: {0}x{1}, {2} mip levels, HDR: {3}",
			m_Width, m_Height, m_MipLevels, m_IsHDR ? "Yes" : "No");
	}
	
	// ========================================
	// CREATE EMPTY CUBEMAP
	// ========================================
	
	void OpenGLTextureCube::CreateEmpty(uint32_t size, bool hdr, uint32_t mipLevels) {
		m_Width = size;
		m_Height = size;
		m_IsHDR = hdr;
		
		// Calculate mip levels if not specified
		if (mipLevels == 0) {
			m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(size))) + 1;
		} else {
			m_MipLevels = mipLevels;
		}
		
		m_InternalFormat = hdr ? GL_RGBA16F : GL_RGBA8;
		m_DataFormat = GL_RGBA;
		
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		
		// Allocate storage for all faces and mip levels
		for (uint32_t i = 0; i < 6; i++) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_InternalFormat,
				size, size, 0, m_DataFormat, hdr ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
		}
		
		// Set texture parameters
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, 
			m_MipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		m_IsLoaded = true;
		
		LNX_LOG_TRACE("Empty cubemap created: {0}x{1}, {2} mip levels, HDR: {3}",
			m_Width, m_Height, m_MipLevels, m_IsHDR ? "Yes" : "No");
	}
	
	// ========================================
	// GENERATE MIPMAPS
	// ========================================
	
	void OpenGLTextureCube::GenerateMipmaps() {
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
	
	// ========================================
	// HDRI TO CUBEMAP CONVERSION
	// ========================================
	
	Ref<TextureCube> OpenGLTextureCube::CreateFromHDRI(const std::string& hdriPath, uint32_t resolution) {
		LNX_PROFILE_FUNCTION();
		
		// Load HDRI as 2D equirectangular texture
		int width, height, channels;
		stbi_set_flip_vertically_on_load(true);
		float* hdrData = stbi_loadf(hdriPath.c_str(), &width, &height, &channels, 0);
		
		if (!hdrData) {
			LNX_LOG_ERROR("Failed to load HDRI: {0}", hdriPath);
			LNX_LOG_ERROR("  Reason: {0}", stbi_failure_reason());
			return nullptr;
		}
		
		LNX_LOG_INFO("Loading HDRI: {0} ({1}x{2}, {3} channels)", hdriPath, width, height, channels);
		
		// Create equirectangular texture
		GLuint hdrTexture;
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, hdrData);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		stbi_image_free(hdrData);
		
		// Create output cubemap
		Ref<OpenGLTextureCube> cubemap = CreateRef<OpenGLTextureCube>(resolution, true, 0);
		
		// Create framebuffer for rendering to cubemap faces
		GLuint captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);
		
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
		
		// Load equirectangular to cubemap conversion shader
		// This shader is embedded as a string for simplicity
		static const char* equirectToCubeVertSrc = R"(
			#version 450 core
			layout (location = 0) in vec3 a_Position;
			out vec3 v_LocalPos;
			uniform mat4 u_Projection;
			uniform mat4 u_View;
			void main() {
				v_LocalPos = a_Position;
				gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
			}
		)";
		
		static const char* equirectToCubeFragSrc = R"(
			#version 450 core
			out vec4 FragColor;
			in vec3 v_LocalPos;
			uniform sampler2D u_EquirectangularMap;
			const vec2 invAtan = vec2(0.1591, 0.3183);
			vec2 SampleSphericalMap(vec3 v) {
				vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
				uv *= invAtan;
				uv += 0.5;
				return uv;
			}
			void main() {
				vec2 uv = SampleSphericalMap(normalize(v_LocalPos));
				vec3 color = texture(u_EquirectangularMap, uv).rgb;
				FragColor = vec4(color, 1.0);
			}
		)";
		
		// Compile shader
		GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertShader, 1, &equirectToCubeVertSrc, nullptr);
		glCompileShader(vertShader);
		
		GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragShader, 1, &equirectToCubeFragSrc, nullptr);
		glCompileShader(fragShader);
		
		GLuint shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertShader);
		glAttachShader(shaderProgram, fragShader);
		glLinkProgram(shaderProgram);
		
		glDeleteShader(vertShader);
		glDeleteShader(fragShader);
		
		// Create cube VAO for rendering
		float cubeVertices[] = {
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
		
		GLuint cubeVAO, cubeVBO;
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		glBindVertexArray(cubeVAO);
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		
		// Setup projection and view matrices for each face
		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] = {
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};
		
		// Render to each face
		glUseProgram(shaderProgram);
		glUniform1i(glGetUniformLocation(shaderProgram, "u_EquirectangularMap"), 0);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_Projection"), 1, GL_FALSE, &captureProjection[0][0]);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		
		glViewport(0, 0, resolution, resolution);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		
		for (uint32_t i = 0; i < 6; i++) {
			glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_View"), 1, GL_FALSE, &captureViews[i][0][0]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap->GetRendererID(), 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			glBindVertexArray(cubeVAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		// Generate mipmaps
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap->GetRendererID());
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		cubemap->m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(resolution))) + 1;
		
		// Cleanup
		glDeleteTextures(1, &hdrTexture);
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
		glDeleteProgram(shaderProgram);
		glDeleteVertexArrays(1, &cubeVAO);
		glDeleteBuffers(1, &cubeVBO);
		
		LNX_LOG_INFO("HDRI converted to cubemap: {0}x{1}, {2} mip levels",
			resolution, resolution, cubemap->m_MipLevels);
		
		return cubemap;
	}
	
	// ========================================
	// IRRADIANCE MAP GENERATION
	// ========================================
	
	Ref<TextureCube> OpenGLTextureCube::GenerateIrradianceMap(uint32_t resolution) const {
		LNX_PROFILE_FUNCTION();
		
		if (!m_IsLoaded) {
			LNX_LOG_ERROR("Cannot generate irradiance map from unloaded cubemap");
			return nullptr;
		}
		
		Ref<OpenGLTextureCube> irradianceMap = CreateRef<OpenGLTextureCube>(resolution, true, 1);
		
		// Irradiance convolution shader (embedded for simplicity)
		static const char* irradianceVertSrc = R"(
			#version 450 core
			layout (location = 0) in vec3 a_Position;
			out vec3 v_LocalPos;
			uniform mat4 u_Projection;
			uniform mat4 u_View;
			void main() {
				v_LocalPos = a_Position;
				gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
			}
		)";
		
		static const char* irradianceFragSrc = R"(
			#version 450 core
			out vec4 FragColor;
			in vec3 v_LocalPos;
			uniform samplerCube u_EnvironmentMap;
			const float PI = 3.14159265359;
			void main() {
				vec3 normal = normalize(v_LocalPos);
				vec3 irradiance = vec3(0.0);
				
				vec3 up = vec3(0.0, 1.0, 0.0);
				vec3 right = normalize(cross(up, normal));
				up = normalize(cross(normal, right));
				
				float sampleDelta = 0.025;
				float nrSamples = 0.0;
				
				for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
					for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
						vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
						vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
						irradiance += texture(u_EnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
						nrSamples++;
					}
				}
				irradiance = PI * irradiance * (1.0 / float(nrSamples));
				FragColor = vec4(irradiance, 1.0);
			}
		)";
		
		// Compile shader
		GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertShader, 1, &irradianceVertSrc, nullptr);
		glCompileShader(vertShader);
		
		GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragShader, 1, &irradianceFragSrc, nullptr);
		glCompileShader(fragShader);
		
		GLuint shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertShader);
		glAttachShader(shaderProgram, fragShader);
		glLinkProgram(shaderProgram);
		
		glDeleteShader(vertShader);
		glDeleteShader(fragShader);
		
		// Create cube geometry
		float cubeVertices[] = {
			-1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
		};
		
		GLuint cubeVAO, cubeVBO;
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		glBindVertexArray(cubeVAO);
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		
		// Framebuffer
		GLuint captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
		
		// View matrices for each face
		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] = {
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};
		
		// Render
		glUseProgram(shaderProgram);
		glUniform1i(glGetUniformLocation(shaderProgram, "u_EnvironmentMap"), 0);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_Projection"), 1, GL_FALSE, &captureProjection[0][0]);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		
		glViewport(0, 0, resolution, resolution);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		
		for (uint32_t i = 0; i < 6; i++) {
			glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_View"), 1, GL_FALSE, &captureViews[i][0][0]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap->GetRendererID(), 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			glBindVertexArray(cubeVAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		// Cleanup
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
		glDeleteProgram(shaderProgram);
		glDeleteVertexArrays(1, &cubeVAO);
		glDeleteBuffers(1, &cubeVBO);
		
		LNX_LOG_INFO("Irradiance map generated: {0}x{1}", resolution, resolution);
		
		return irradianceMap;
	}
	
	// ========================================
	// PREFILTERED MAP GENERATION
	// ========================================
	
	Ref<TextureCube> OpenGLTextureCube::GeneratePrefilteredMap(uint32_t resolution) const {
		LNX_PROFILE_FUNCTION();
		
		if (!m_IsLoaded) {
			LNX_LOG_ERROR("Cannot generate prefiltered map from unloaded cubemap");
			return nullptr;
		}
		
		uint32_t maxMipLevels = 5;
		Ref<OpenGLTextureCube> prefilteredMap = CreateRef<OpenGLTextureCube>(resolution, true, maxMipLevels);
		
		// Prefilter shader for split-sum approximation
		static const char* prefilterVertSrc = R"(
			#version 450 core
			layout (location = 0) in vec3 a_Position;
			out vec3 v_LocalPos;
			uniform mat4 u_Projection;
			uniform mat4 u_View;
			void main() {
				v_LocalPos = a_Position;
				gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
			}
		)";
		
		static const char* prefilterFragSrc = R"(
			#version 450 core
			out vec4 FragColor;
			in vec3 v_LocalPos;
			uniform samplerCube u_EnvironmentMap;
			uniform float u_Roughness;
			const float PI = 3.14159265359;
			
			float RadicalInverse_VdC(uint bits) {
				bits = (bits << 16u) | (bits >> 16u);
				bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
				bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
				bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
				bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
				return float(bits) * 2.3283064365386963e-10;
			}
			
			vec2 Hammersley(uint i, uint N) {
				return vec2(float(i)/float(N), RadicalInverse_VdC(i));
			}
			
			vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
				float a = roughness * roughness;
				float phi = 2.0 * PI * Xi.x;
				float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
				float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
				
				vec3 H;
				H.x = cos(phi) * sinTheta;
				H.y = sin(phi) * sinTheta;
				H.z = cosTheta;
				
				vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
				vec3 tangent = normalize(cross(up, N));
				vec3 bitangent = cross(N, tangent);
				
				return normalize(tangent * H.x + bitangent * H.y + N * H.z);
			}
			
			void main() {
				vec3 N = normalize(v_LocalPos);
				vec3 R = N;
				vec3 V = R;
				
				const uint SAMPLE_COUNT = 1024u;
				float totalWeight = 0.0;
				vec3 prefilteredColor = vec3(0.0);
				
				for (uint i = 0u; i < SAMPLE_COUNT; i++) {
					vec2 Xi = Hammersley(i, SAMPLE_COUNT);
					vec3 H = ImportanceSampleGGX(Xi, N, u_Roughness);
					vec3 L = normalize(2.0 * dot(V, H) * H - V);
					float NdotL = max(dot(N, L), 0.0);
					
					if (NdotL > 0.0) {
						prefilteredColor += texture(u_EnvironmentMap, L).rgb * NdotL;
						totalWeight += NdotL;
					}
				}
				
				prefilteredColor = prefilteredColor / totalWeight;
				FragColor = vec4(prefilteredColor, 1.0);
			}
		)";
		
		// Compile shader
		GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertShader, 1, &prefilterVertSrc, nullptr);
		glCompileShader(vertShader);
		
		GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragShader, 1, &prefilterFragSrc, nullptr);
		glCompileShader(fragShader);
		
		GLuint shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertShader);
		glAttachShader(shaderProgram, fragShader);
		glLinkProgram(shaderProgram);
		
		glDeleteShader(vertShader);
		glDeleteShader(fragShader);
		
		// Create cube geometry
		float cubeVertices[] = {
			-1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
		};
		
		GLuint cubeVAO, cubeVBO;
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		glBindVertexArray(cubeVAO);
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		
		// Framebuffer
		GLuint captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		
		// View matrices
		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] = {
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};
		
		glUseProgram(shaderProgram);
		glUniform1i(glGetUniformLocation(shaderProgram, "u_EnvironmentMap"), 0);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_Projection"), 1, GL_FALSE, &captureProjection[0][0]);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		
		// Render each mip level
		for (uint32_t mip = 0; mip < maxMipLevels; mip++) {
			uint32_t mipWidth = static_cast<uint32_t>(resolution * std::pow(0.5f, mip));
			uint32_t mipHeight = mipWidth;
			
			glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
			glViewport(0, 0, mipWidth, mipHeight);
			
			float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
			glUniform1f(glGetUniformLocation(shaderProgram, "u_Roughness"), roughness);
			
			for (uint32_t i = 0; i < 6; i++) {
				glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_View"), 1, GL_FALSE, &captureViews[i][0][0]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilteredMap->GetRendererID(), mip);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				
				glBindVertexArray(cubeVAO);
				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		// Cleanup
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
		glDeleteProgram(shaderProgram);
		glDeleteVertexArrays(1, &cubeVAO);
		glDeleteBuffers(1, &cubeVBO);
		
		prefilteredMap->m_MipLevels = maxMipLevels;
		
		LNX_LOG_INFO("Prefiltered environment map generated: {0}x{1}, {2} mip levels",
			resolution, resolution, maxMipLevels);
		
		return prefilteredMap;
	}
	
}
