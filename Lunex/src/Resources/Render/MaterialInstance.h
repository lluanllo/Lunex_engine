#pragma once

/**
 * @file MaterialInstance.h
 * @brief Runtime material instance with override support
 * 
 * AAA Architecture: MaterialInstance lives in Resources/Render/
 * This is the runtime GPU representation that can have local overrides.
 */

#include "Core/Core.h"
#include "Assets/Materials/MaterialAsset.h"
#include <memory>
#include <optional>

namespace Lunex {

	/**
	 * @class MaterialInstance
	 * @brief Runtime material instance with local property overrides
	 * 
	 * Allows per-entity material property overrides without modifying the base asset.
	 */
	class MaterialInstance {
	public:
		MaterialInstance(Ref<MaterialAsset> baseAsset);
		
		static Ref<MaterialInstance> Create(const std::filesystem::path& assetPath);
		static Ref<MaterialInstance> Create(Ref<MaterialAsset> baseAsset);
		
		Ref<MaterialInstance> Clone() const;

		~MaterialInstance() = default;

		// ========== ASSET BASE ==========
		
		Ref<MaterialAsset> GetBaseAsset() const { return m_BaseAsset; }
		void SetBaseAsset(Ref<MaterialAsset> asset);
		bool HasLocalOverrides() const { return m_HasLocalOverrides; }
		void ResetOverrides();

		// ========== PBR PROPERTIES ==========
		
		glm::vec4 GetAlbedo() const;
		void SetAlbedo(const glm::vec4& color, bool asOverride = false);

		float GetMetallic() const;
		void SetMetallic(float metallic, bool asOverride = false);

		float GetRoughness() const;
		void SetRoughness(float roughness, bool asOverride = false);

		float GetSpecular() const;
		void SetSpecular(float specular, bool asOverride = false);

		glm::vec3 GetEmissionColor() const;
		void SetEmissionColor(const glm::vec3& color, bool asOverride = false);

		float GetEmissionIntensity() const;
		void SetEmissionIntensity(float intensity, bool asOverride = false);

		float GetNormalIntensity() const;
		void SetNormalIntensity(float intensity, bool asOverride = false);

		// ========== TEXTURES (from base asset) ==========
		
		Ref<Texture2D> GetAlbedoMap() const { return m_BaseAsset->GetAlbedoMap(); }
		Ref<Texture2D> GetNormalMap() const { return m_BaseAsset->GetNormalMap(); }
		Ref<Texture2D> GetMetallicMap() const { return m_BaseAsset->GetMetallicMap(); }
		Ref<Texture2D> GetRoughnessMap() const { return m_BaseAsset->GetRoughnessMap(); }
		Ref<Texture2D> GetSpecularMap() const { return m_BaseAsset->GetSpecularMap(); }
		Ref<Texture2D> GetEmissionMap() const { return m_BaseAsset->GetEmissionMap(); }
		Ref<Texture2D> GetAOMap() const { return m_BaseAsset->GetAOMap(); }

		bool HasAlbedoMap() const { return m_BaseAsset->HasAlbedoMap(); }
		bool HasNormalMap() const { return m_BaseAsset->HasNormalMap(); }
		bool HasMetallicMap() const { return m_BaseAsset->HasMetallicMap(); }
		bool HasRoughnessMap() const { return m_BaseAsset->HasRoughnessMap(); }
		bool HasSpecularMap() const { return m_BaseAsset->HasSpecularMap(); }
		bool HasEmissionMap() const { return m_BaseAsset->HasEmissionMap(); }
		bool HasAOMap() const { return m_BaseAsset->HasAOMap(); }

		// Detail normals & layered (from base asset)
		const std::vector<DetailNormalMap>& GetDetailNormalMaps() const { return m_BaseAsset->GetDetailNormalMaps(); }
		bool HasDetailNormalMaps() const { return m_BaseAsset->HasDetailNormalMaps(); }
		bool HasLayeredTexture() const { return m_BaseAsset->HasLayeredTexture(); }
		const LayeredTextureConfig& GetLayeredTextureConfig() const { return m_BaseAsset->GetLayeredTextureConfig(); }

		// Multipliers
		float GetMetallicMultiplier() const;
		void SetMetallicMultiplier(float multiplier, bool asOverride = false);

		float GetRoughnessMultiplier() const;
		void SetRoughnessMultiplier(float multiplier, bool asOverride = false);

		float GetSpecularMultiplier() const;
		void SetSpecularMultiplier(float multiplier, bool asOverride = false);

		float GetAOMultiplier() const;
		void SetAOMultiplier(float multiplier, bool asOverride = false);

		// ========== RENDER DATA ==========
		
		MaterialAsset::MaterialUniformData GetUniformData() const;
		void BindTextures() const;

		// ========== INFO ==========
		
		const std::string& GetName() const { return m_BaseAsset->GetName(); }
		UUID GetAssetID() const { return m_BaseAsset->GetID(); }
		const std::filesystem::path& GetAssetPath() const { return m_BaseAsset->GetPath(); }

	private:
		Ref<MaterialAsset> m_BaseAsset;
		bool m_HasLocalOverrides = false;

		std::optional<glm::vec4> m_AlbedoOverride;
		std::optional<float> m_MetallicOverride;
		std::optional<float> m_RoughnessOverride;
		std::optional<float> m_SpecularOverride;
		std::optional<glm::vec3> m_EmissionColorOverride;
		std::optional<float> m_EmissionIntensityOverride;
		std::optional<float> m_NormalIntensityOverride;

		std::optional<float> m_MetallicMultiplierOverride;
		std::optional<float> m_RoughnessMultiplierOverride;
		std::optional<float> m_SpecularMultiplierOverride;
		std::optional<float> m_AOMultiplierOverride;

		void MarkAsOverridden() { m_HasLocalOverrides = true; }
	};

} // namespace Lunex
