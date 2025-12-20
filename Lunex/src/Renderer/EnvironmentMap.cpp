#include "stpch.h"
#include "EnvironmentMap.h"
#include "RHI/OpenGL/OpenGLRHITextureCube.h"

#include <glad/glad.h>

namespace Lunex {
	
	// Static member initialization
	Ref<Texture2D> EnvironmentMap::s_SharedBRDFLUT = nullptr;
	
	// ========================================
	// LOADING FROM HDRI
	// ========================================
	
	bool EnvironmentMap::LoadFromHDRI(const std::string& hdriPath, uint32_t resolution) {
		LNX_PROFILE_FUNCTION();
		
		m_IsLoaded = false;
		m_Path = hdriPath;
		
		// Convert HDRI to cubemap
		m_EnvironmentMap = TextureCube::CreateFromHDRI(hdriPath, resolution);
		
		if (!m_EnvironmentMap || !m_EnvironmentMap->IsLoaded()) {
			LNX_LOG_ERROR("Failed to load environment from HDRI: {0}", hdriPath);
			return false;
		}
		
		// Generate IBL maps
		GenerateIBLMaps();
		
		m_IsLoaded = true;
		LNX_LOG_INFO("Environment loaded from HDRI: {0}", hdriPath);
		
		return true;
	}
	
	// ========================================
	// LOADING FROM 6 FACES
	// ========================================
	
	bool EnvironmentMap::LoadFromFaces(const std::array<std::string, 6>& facePaths) {
		LNX_PROFILE_FUNCTION();
		
		m_IsLoaded = false;
		m_Path = facePaths[0]; // Use first face path as identifier
		
		m_EnvironmentMap = TextureCube::Create(facePaths);
		
		if (!m_EnvironmentMap || !m_EnvironmentMap->IsLoaded()) {
			LNX_LOG_ERROR("Failed to load environment from faces");
			return false;
		}
		
		GenerateIBLMaps();
		
		m_IsLoaded = true;
		LNX_LOG_INFO("Environment loaded from {0} faces", 6);
		
		return true;
	}
	
	// ========================================
	// SET EXISTING CUBEMAP
	// ========================================
	
	void EnvironmentMap::SetEnvironmentMap(Ref<TextureCube> envMap) {
		if (!envMap || !envMap->IsLoaded()) {
			LNX_LOG_WARN("Cannot set invalid environment map");
			return;
		}
		
		m_EnvironmentMap = envMap;
		GenerateIBLMaps();
		m_IsLoaded = true;
	}
	
	// ========================================
	// GENERATE IBL MAPS
	// ========================================
	
	void EnvironmentMap::GenerateIBLMaps() {
		LNX_PROFILE_FUNCTION();
		
		if (!m_EnvironmentMap) {
			return;
		}
		
		// Cast to OpenGL implementation for IBL generation
		auto glEnvMap = std::dynamic_pointer_cast<OpenGLTextureCube>(m_EnvironmentMap);
		if (!glEnvMap) {
			LNX_LOG_ERROR("EnvironmentMap::GenerateIBLMaps - Invalid cubemap type");
			return;
		}
		
		// Generate irradiance map (for diffuse IBL)
		m_IrradianceMap = glEnvMap->GenerateIrradianceMap(32);
		
		// Generate prefiltered map (for specular IBL)
		m_PrefilteredMap = glEnvMap->GeneratePrefilteredMap(128);
		
		// Get or create BRDF LUT
		m_BRDFLUT = GetSharedBRDFLUT();
		
		LNX_LOG_INFO("IBL maps generated: Irradiance ({0}x{0}), Prefiltered ({1}x{1})",
			m_IrradianceMap ? m_IrradianceMap->GetWidth() : 0,
			m_PrefilteredMap ? m_PrefilteredMap->GetWidth() : 0);
	}
	
	// ========================================
	// BINDING
	// ========================================
	
	void EnvironmentMap::Bind(uint32_t envSlot, uint32_t irradianceSlot, 
							   uint32_t prefilteredSlot, uint32_t brdfLutSlot) const {
		if (m_EnvironmentMap) {
			m_EnvironmentMap->Bind(envSlot);
		}
		if (m_IrradianceMap) {
			m_IrradianceMap->Bind(irradianceSlot);
		}
		if (m_PrefilteredMap) {
			m_PrefilteredMap->Bind(prefilteredSlot);
		}
		if (m_BRDFLUT) {
			m_BRDFLUT->Bind(brdfLutSlot);
		}
	}
	
	void EnvironmentMap::Unbind() const {
		if (m_EnvironmentMap) {
			m_EnvironmentMap->Unbind();
		}
	}
	
	// ========================================
	// BRDF LUT GENERATION
	// ========================================
	
	Ref<Texture2D> EnvironmentMap::GenerateBRDFLUT(uint32_t resolution) {
		LNX_PROFILE_FUNCTION();
		
		// BRDF integration shader for split-sum approximation
		static const char* brdfVertSrc = R"(
			#version 450 core
			layout (location = 0) in vec3 a_Position;
			layout (location = 1) in vec2 a_TexCoords;
			out vec2 v_TexCoords;
			void main() {
				v_TexCoords = a_TexCoords;
				gl_Position = vec4(a_Position, 1.0);
			}
		)";
		
		static const char* brdfFragSrc = R"(
			#version 450 core
			out vec2 FragColor;
			in vec2 v_TexCoords;
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
			
			float GeometrySchlickGGX(float NdotV, float roughness) {
				float a = roughness;
				float k = (a * a) / 2.0;
				return NdotV / (NdotV * (1.0 - k) + k);
			}
			
			float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
				float NdotV = max(dot(N, V), 0.0);
				float NdotL = max(dot(N, L), 0.0);
				return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
			}
			
			vec2 IntegrateBRDF(float NdotV, float roughness) {
				vec3 V;
				V.x = sqrt(1.0 - NdotV * NdotV);
				V.y = 0.0;
				V.z = NdotV;
				
				float A = 0.0;
				float B = 0.0;
				vec3 N = vec3(0.0, 0.0, 1.0);
				
				const uint SAMPLE_COUNT = 1024u;
				for (uint i = 0u; i < SAMPLE_COUNT; i++) {
					vec2 Xi = Hammersley(i, SAMPLE_COUNT);
					vec3 H = ImportanceSampleGGX(Xi, N, roughness);
					vec3 L = normalize(2.0 * dot(V, H) * H - V);
					
					float NdotL = max(L.z, 0.0);
					float NdotH = max(H.z, 0.0);
					float VdotH = max(dot(V, H), 0.0);
					
					if (NdotL > 0.0) {
						float G = GeometrySmith(N, V, L, roughness);
						float G_Vis = (G * VdotH) / (NdotH * NdotV);
						float Fc = pow(1.0 - VdotH, 5.0);
						
						A += (1.0 - Fc) * G_Vis;
						B += Fc * G_Vis;
					}
				}
				A /= float(SAMPLE_COUNT);
				B /= float(SAMPLE_COUNT);
				return vec2(A, B);
			}
			
			void main() {
				FragColor = IntegrateBRDF(v_TexCoords.x, v_TexCoords.y);
			}
		)";
		
		// Compile shader
		GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertShader, 1, &brdfVertSrc, nullptr);
		glCompileShader(vertShader);
		
		GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragShader, 1, &brdfFragSrc, nullptr);
		glCompileShader(fragShader);
		
		GLuint shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertShader);
		glAttachShader(shaderProgram, fragShader);
		glLinkProgram(shaderProgram);
		
		glDeleteShader(vertShader);
		glDeleteShader(fragShader);
		
		// Create quad
		float quadVertices[] = {
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		
		GLuint quadVAO, quadVBO;
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		
		// Create BRDF LUT texture
		GLuint brdfLUTTexture;
		glGenTextures(1, &brdfLUTTexture);
		glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, resolution, resolution, 0, GL_RG, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		// Render to LUT
		GLuint captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);
		
		glViewport(0, 0, resolution, resolution);
		glUseProgram(shaderProgram);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		// Cleanup
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
		glDeleteProgram(shaderProgram);
		glDeleteVertexArrays(1, &quadVAO);
		glDeleteBuffers(1, &quadVBO);
		
		// Wrap in Texture2D (we need a simple wrapper here)
		// For now, we'll create a placeholder - in production you'd wrap this properly
		// The texture ID is stored in brdfLUTTexture
		
		LNX_LOG_INFO("BRDF LUT generated: {0}x{1}", resolution, resolution);
		
		// Create a simple wrapper - this should be a proper Texture2D implementation
		// For now, returning nullptr and the shader will use approximation
		return nullptr;
	}
	
	Ref<Texture2D> EnvironmentMap::GetSharedBRDFLUT() {
		if (!s_SharedBRDFLUT) {
			s_SharedBRDFLUT = GenerateBRDFLUT(512);
		}
		return s_SharedBRDFLUT;
	}
	
}
