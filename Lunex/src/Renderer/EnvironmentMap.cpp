#include "stpch.h"
#include "EnvironmentMap.h"
#include "RHI/OpenGL/OpenGLRHITextureCube.h"

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
		
		// TODO: Implement BRDF LUT generation using the RHI abstraction layer.
		// This requires creating a dedicated BRDF integration shader as a .glsl file
		// and rendering via the engine's Shader/VertexArray/Framebuffer abstractions
		// instead of raw OpenGL calls.
		// For now, the shader will use an analytical approximation as a fallback.
		
		LNX_LOG_WARN("EnvironmentMap: BRDF LUT generation not yet implemented via RHI. Using shader approximation.");
		
		return nullptr;
	}
	
	Ref<Texture2D> EnvironmentMap::GetSharedBRDFLUT() {
		if (!s_SharedBRDFLUT) {
			s_SharedBRDFLUT = GenerateBRDFLUT(512);
		}
		return s_SharedBRDFLUT;
	}
	
}
