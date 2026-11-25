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
	
	// Split-sum BRDF integration (Karis 2013), computed on the CPU. The GPU version rendered
	// into a raw GL texture that was never wrapped and returned nullptr, so specular IBL was
	// always multiplied by zero. x = NdotV, y = roughness; R = scale, G = bias.
	Ref<Texture2D> EnvironmentMap::GenerateBRDFLUT(uint32_t resolution) {
		LNX_PROFILE_FUNCTION();
		resolution = std::max(16u, std::min(resolution, 256u));
		const uint32_t sampleCount = 512;

		auto radicalInverse = [](uint32_t bits) {
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return static_cast<float>(bits) * 2.3283064365386963e-10f;
		};
		auto geometrySchlick = [](float NdotX, float roughness) {
			float k = (roughness * roughness) / 2.0f;   // IBL remapping
			return NdotX / (NdotX * (1.0f - k) + k);
		};

		Texture2D::DecodedImage image;
		image.Width = static_cast<int>(resolution);
		image.Height = static_cast<int>(resolution);
		image.Channels = 4;
		image.Pixels.resize(static_cast<size_t>(resolution) * resolution * 4);

		for (uint32_t y = 0; y < resolution; y++) {
			float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution);
			float a = roughness * roughness;
			for (uint32_t x = 0; x < resolution; x++) {
				float NdotV = std::max((static_cast<float>(x) + 0.5f) / static_cast<float>(resolution), 1e-4f);
				glm::vec3 V(std::sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);

				float A = 0.0f, B = 0.0f;
				for (uint32_t i = 0; i < sampleCount; i++) {
					float xi1 = static_cast<float>(i) / static_cast<float>(sampleCount);
					float xi2 = radicalInverse(i);
					float phi = 2.0f * glm::pi<float>() * xi1;
					float cosTheta = std::sqrt((1.0f - xi2) / (1.0f + (a * a - 1.0f) * xi2));
					float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
					glm::vec3 H(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
					glm::vec3 L = 2.0f * glm::dot(V, H) * H - V;

					float NdotL = std::max(L.z, 0.0f);
					float NdotH = std::max(H.z, 0.0f);
					float VdotH = std::max(glm::dot(V, H), 0.0f);
					if (NdotL > 0.0f) {
						float G = geometrySchlick(NdotV, roughness) * geometrySchlick(NdotL, roughness);
						float Gvis = (G * VdotH) / std::max(NdotH * NdotV, 1e-6f);
						float Fc = std::pow(1.0f - VdotH, 5.0f);
						A += (1.0f - Fc) * Gvis;
						B += Fc * Gvis;
					}
				}
				A /= static_cast<float>(sampleCount);
				B /= static_cast<float>(sampleCount);

				uint8_t* px = &image.Pixels[(static_cast<size_t>(y) * resolution + x) * 4];
				px[0] = static_cast<uint8_t>(glm::clamp(A, 0.0f, 1.0f) * 255.0f + 0.5f);
				px[1] = static_cast<uint8_t>(glm::clamp(B, 0.0f, 1.0f) * 255.0f + 0.5f);
				px[2] = 0;
				px[3] = 255;
			}
		}

		auto lut = Texture2D::CreateFromDecoded(image);
		LNX_LOG_INFO("BRDF LUT generated: {0}x{0} (CPU, {1} samples)", resolution, sampleCount);
		return lut;
	}

	Ref<Texture2D> EnvironmentMap::GetSharedBRDFLUT() {
		if (!s_SharedBRDFLUT) {
			s_SharedBRDFLUT = GenerateBRDFLUT(128);
		}
		return s_SharedBRDFLUT;
	}
	
}
