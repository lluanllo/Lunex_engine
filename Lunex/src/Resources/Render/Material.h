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

			int UseAlbedoMap;
			int UseNormalMap;
			int UseMetallicMap;
			int UseRoughnessMap;
			int UseSpecularMap;
			int UseEmissionMap;
			int UseAOMap;
			int UseLayeredMap;

			float MetallicMultiplier;
			float RoughnessMultiplier;
			float SpecularMultiplier;
			float AOMultiplier;

			glm::vec2 UVTiling;
			glm::vec2 UVOffset;

			int LayeredChannelMetallic;
			int LayeredChannelRoughness;
			int LayeredChannelAO;
			int UseHeightMap;

			float HeightScale;
			int UseDetailNormalMap;
			float DetailNormalScale;
			float AlphaCutoff;

			glm::vec2 DetailUVTiling;
			int AlphaMode;
			int FlipNormalMapY;

			int AlbedoColorSpace;
			int NormalColorSpace;
			int LayeredColorSpace;
			int EmissionColorSpace;
		};

		MaterialData GetMaterialData(const glm::vec3& viewPos, bool hasAlbedo, bool hasNormal,
			bool hasMetallic, bool hasRoughness, bool hasSpecular,
			bool hasEmission, bool hasAO,
			float metallicMult, float roughnessMult,
			float specularMult, float aoMult) const {
			MaterialData data{};
			data.Color = m_Color;
			data.Metallic = m_Metallic;
			data.Roughness = m_Roughness;
			data.Specular = m_Specular;
			data.EmissionIntensity = m_EmissionIntensity;
			data.EmissionColor = m_EmissionColor;
			data.NormalIntensity = 1.0f;
			data.UseAlbedoMap = hasAlbedo ? 1 : 0;
			data.UseNormalMap = hasNormal ? 1 : 0;
			data.UseMetallicMap = hasMetallic ? 1 : 0;
			data.UseRoughnessMap = hasRoughness ? 1 : 0;
			data.UseSpecularMap = hasSpecular ? 1 : 0;
			data.UseEmissionMap = hasEmission ? 1 : 0;
			data.UseAOMap = hasAO ? 1 : 0;
			data.UseLayeredMap = 0;
			data.MetallicMultiplier = metallicMult;
			data.RoughnessMultiplier = roughnessMult;
			data.SpecularMultiplier = specularMult;
			data.AOMultiplier = aoMult;
			data.UVTiling = glm::vec2(1.0f, 1.0f);
			data.UVOffset = glm::vec2(0.0f, 0.0f);
			data.LayeredChannelMetallic = 0;
			data.LayeredChannelRoughness = 1;
			data.LayeredChannelAO = 2;
			data.UseHeightMap = 0;
			data.HeightScale = 0.05f;
			data.UseDetailNormalMap = 0;
			data.DetailNormalScale = 1.0f;
			data.AlphaCutoff = 0.5f;
			data.DetailUVTiling = glm::vec2(4.0f, 4.0f);
			data.AlphaMode = 0;
			data.FlipNormalMapY = 0;
			data.AlbedoColorSpace = 0;  // sRGB
			data.NormalColorSpace = 1;   // Linear
			data.LayeredColorSpace = 1;  // Linear
			data.EmissionColorSpace = 0; // sRGB
			return data;
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
