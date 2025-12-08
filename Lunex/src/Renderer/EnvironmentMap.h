#pragma once

#include "Core/Core.h"
#include "Renderer/TextureCube.h"
#include "Renderer/Texture.h"

#include <string>
#include <glm/glm.hpp>

namespace Lunex {
	
	/**
	 * EnvironmentMap - Manages environment lighting for IBL (Image Based Lighting)
	 * 
	 * Contains:
	 *   - Environment cubemap (the skybox texture)
	 *   - Irradiance map (diffuse IBL)
	 *   - Prefiltered map (specular IBL, mipmapped for roughness)
	 *   - BRDF LUT (for split-sum approximation)
	 */
	class EnvironmentMap {
	public:
		EnvironmentMap() = default;
		~EnvironmentMap() = default;
		
		// ========================================
		// LOADING
		// ========================================
		
		/**
		 * Load environment from an HDRI file
		 * This will automatically generate:
		 *   - Environment cubemap
		 *   - Irradiance map
		 *   - Prefiltered map
		 */
		bool LoadFromHDRI(const std::string& hdriPath, uint32_t resolution = 1024);
		
		/**
		 * Load environment from 6 cubemap faces
		 * Face order: +X, -X, +Y, -Y, +Z, -Z
		 */
		bool LoadFromFaces(const std::array<std::string, 6>& facePaths);
		
		/**
		 * Set a pre-existing cubemap as the environment
		 * Will generate irradiance and prefiltered maps
		 */
		void SetEnvironmentMap(Ref<TextureCube> envMap);
		
		// ========================================
		// ACCESSORS
		// ========================================
		
		Ref<TextureCube> GetEnvironmentMap() const { return m_EnvironmentMap; }
		Ref<TextureCube> GetIrradianceMap() const { return m_IrradianceMap; }
		Ref<TextureCube> GetPrefilteredMap() const { return m_PrefilteredMap; }
		Ref<Texture2D> GetBRDFLUT() const { return m_BRDFLUT; }
		
		bool IsLoaded() const { return m_IsLoaded; }
		const std::string& GetPath() const { return m_Path; }
		
		// ========================================
		// ENVIRONMENT SETTINGS
		// ========================================
		
		// Intensity multiplier for the environment
		void SetIntensity(float intensity) { m_Intensity = intensity; }
		float GetIntensity() const { return m_Intensity; }
		
		// Rotation (in radians around Y axis)
		void SetRotation(float rotation) { m_Rotation = rotation; }
		float GetRotation() const { return m_Rotation; }
		
		// Tint color
		void SetTint(const glm::vec3& tint) { m_Tint = tint; }
		const glm::vec3& GetTint() const { return m_Tint; }
		
		// Blur level (0 = sharp, 1 = fully blurred)
		void SetBlur(float blur) { m_Blur = glm::clamp(blur, 0.0f, 1.0f); }
		float GetBlur() const { return m_Blur; }
		
		// ========================================
		// BINDING
		// ========================================
		
		/**
		 * Bind all environment textures for IBL rendering
		 * @param envSlot Texture slot for environment map
		 * @param irradianceSlot Texture slot for irradiance map
		 * @param prefilteredSlot Texture slot for prefiltered map
		 * @param brdfLutSlot Texture slot for BRDF LUT
		 */
		void Bind(uint32_t envSlot = 7, uint32_t irradianceSlot = 8, 
				  uint32_t prefilteredSlot = 9, uint32_t brdfLutSlot = 10) const;
		
		void Unbind() const;
		
		// ========================================
		// STATIC UTILITIES
		// ========================================
		
		/**
		 * Generate BRDF LUT texture (only needs to be done once)
		 * This is a 2D texture used for the split-sum approximation
		 */
		static Ref<Texture2D> GenerateBRDFLUT(uint32_t resolution = 512);
		
		/**
		 * Get or create the shared BRDF LUT
		 */
		static Ref<Texture2D> GetSharedBRDFLUT();
		
	private:
		void GenerateIBLMaps();
		
	private:
		Ref<TextureCube> m_EnvironmentMap;
		Ref<TextureCube> m_IrradianceMap;
		Ref<TextureCube> m_PrefilteredMap;
		Ref<Texture2D> m_BRDFLUT;
		
		std::string m_Path;
		bool m_IsLoaded = false;
		
		// Settings
		float m_Intensity = 1.0f;
		float m_Rotation = 0.0f;
		glm::vec3 m_Tint = glm::vec3(1.0f);
		float m_Blur = 0.0f;
		
		// Shared BRDF LUT
		static Ref<Texture2D> s_SharedBRDFLUT;
	};
	
}
