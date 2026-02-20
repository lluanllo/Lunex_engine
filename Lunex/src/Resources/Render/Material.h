#pragma once

/**
 * @file Material.h
 * @brief GPU-facing material for rendering
 * 
 * AAA Architecture: Material lives in Resources/Render/
 * This is the runtime GPU resource, not the asset.
 */

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include <glm/glm.hpp>

namespace Lunex {

	/**
	 * @class Material
	 * @brief GPU-ready material data for rendering
	 * 
	 * This is the runtime representation used by the renderer.
	 * Created from MaterialAsset or MaterialInstance.
	 */
	class Material {
	public:
		Material();
		Material(const glm::vec4& color);
		~Material() = default;

		// ========== PBR PROPERTIES ==========
		
		void SetColor(const glm::vec4& color) { m_Color = color; }
		void SetMetallic(float metallic) { m_Metallic = glm::clamp(metallic, 0.0f, 1.0f); }
		void SetRoughness(float roughness) { m_Roughness = glm::clamp(roughness, 0.0f, 1.0f); }
		void SetSpecular(float specular) { m_Specular = glm::clamp(specular, 0.0f, 1.0f); }
		void SetEmissionColor(const glm::vec3& color) { m_EmissionColor = color; }
		void SetEmissionIntensity(float intensity) { m_EmissionIntensity = glm::max(0.0f, intensity); }

		const glm::vec4& GetColor() const { return m_Color; }
		float GetMetallic() const { return m_Metallic; }
		float GetRoughness() const { return m_Roughness; }
		float GetSpecular() const { return m_Specular; }
		const glm::vec3& GetEmissionColor() const { return m_EmissionColor; }
		float GetEmissionIntensity() const { return m_EmissionIntensity; }

		// ========== GPU DATA ==========
		
		struct MaterialData {
			glm::vec4 Color;
			float Metallic;
			float Roughness;
			float Specular;
			float EmissionIntensity;
			glm::vec3 EmissionColor;
			float NormalIntensity;
			glm::vec3 ViewPos;
			
			int UseAlbedoMap;
			int UseNormalMap;
			int UseMetallicMap;
			int UseRoughnessMap;
			int UseSpecularMap;
			int UseEmissionMap;
			int UseAOMap;
			float _padding2;
			
			float MetallicMultiplier;
			float RoughnessMultiplier;
			float SpecularMultiplier;
			float AOMultiplier;
		};

		MaterialData GetMaterialData(const glm::vec3& viewPos, bool hasAlbedo, bool hasNormal, 
									 bool hasMetallic, bool hasRoughness, bool hasSpecular,
									 bool hasEmission, bool hasAO,
									 float metallicMult, float roughnessMult, 
									 float specularMult, float aoMult) const {
			return {
				m_Color,
				m_Metallic,
				m_Roughness,
				m_Specular,
				m_EmissionIntensity,
				m_EmissionColor,
				1.0f,
				viewPos,
				hasAlbedo ? 1 : 0,
				hasNormal ? 1 : 0,
				hasMetallic ? 1 : 0,
				hasRoughness ? 1 : 0,
				hasSpecular ? 1 : 0,
				hasEmission ? 1 : 0,
				hasAO ? 1 : 0,
				0.0f,
				metallicMult,
				roughnessMult,
				specularMult,
				aoMult
			};
		}

	private:
		glm::vec4 m_Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float m_Metallic = 0.0f;
		float m_Roughness = 0.5f;
		float m_Specular = 0.5f;
		glm::vec3 m_EmissionColor = { 0.0f, 0.0f, 0.0f };
		float m_EmissionIntensity = 0.0f;
	};

} // namespace Lunex
