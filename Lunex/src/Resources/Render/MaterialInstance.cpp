#include "stpch.h"
#include "MaterialInstance.h"
#include "Log/Log.h"

#include <glad/glad.h>

namespace Lunex {

	MaterialInstance::MaterialInstance(Ref<MaterialAsset> baseAsset)
		: m_BaseAsset(baseAsset)
	{
		LNX_CORE_ASSERT(baseAsset, "MaterialInstance - Base asset cannot be null");
	}

	Ref<MaterialInstance> MaterialInstance::Create(const std::filesystem::path& assetPath) {
		Ref<MaterialAsset> asset = MaterialAsset::LoadFromFile(assetPath);
		if (!asset) {
			LNX_LOG_ERROR("MaterialInstance::Create - Failed to load MaterialAsset from: {0}", assetPath.string());
			return nullptr;
		}
		return CreateRef<MaterialInstance>(asset);
	}

	Ref<MaterialInstance> MaterialInstance::Create(Ref<MaterialAsset> baseAsset) {
		if (!baseAsset) {
			LNX_LOG_ERROR("MaterialInstance::Create - Base asset is null");
			return nullptr;
		}
		return CreateRef<MaterialInstance>(baseAsset);
	}

	Ref<MaterialInstance> MaterialInstance::Clone() const {
		auto clone = CreateRef<MaterialInstance>(m_BaseAsset);
		
		clone->m_HasLocalOverrides = m_HasLocalOverrides;
		clone->m_AlbedoOverride = m_AlbedoOverride;
		clone->m_MetallicOverride = m_MetallicOverride;
		clone->m_RoughnessOverride = m_RoughnessOverride;
		clone->m_SpecularOverride = m_SpecularOverride;
		clone->m_EmissionColorOverride = m_EmissionColorOverride;
		clone->m_EmissionIntensityOverride = m_EmissionIntensityOverride;
		clone->m_NormalIntensityOverride = m_NormalIntensityOverride;
		clone->m_MetallicMultiplierOverride = m_MetallicMultiplierOverride;
		clone->m_RoughnessMultiplierOverride = m_RoughnessMultiplierOverride;
		clone->m_SpecularMultiplierOverride = m_SpecularMultiplierOverride;
		clone->m_AOMultiplierOverride = m_AOMultiplierOverride;
		
		return clone;
	}

	void MaterialInstance::SetBaseAsset(Ref<MaterialAsset> asset) {
		LNX_CORE_ASSERT(asset, "MaterialInstance::SetBaseAsset - Asset cannot be null");
		m_BaseAsset = asset;
		ResetOverrides();
	}

	void MaterialInstance::ResetOverrides() {
		m_HasLocalOverrides = false;
		m_AlbedoOverride.reset();
		m_MetallicOverride.reset();
		m_RoughnessOverride.reset();
		m_SpecularOverride.reset();
		m_EmissionColorOverride.reset();
		m_EmissionIntensityOverride.reset();
		m_NormalIntensityOverride.reset();
		m_MetallicMultiplierOverride.reset();
		m_RoughnessMultiplierOverride.reset();
		m_SpecularMultiplierOverride.reset();
		m_AOMultiplierOverride.reset();
	}

	glm::vec4 MaterialInstance::GetAlbedo() const {
		return m_AlbedoOverride.value_or(m_BaseAsset->GetAlbedo());
	}

	void MaterialInstance::SetAlbedo(const glm::vec4& color, bool asOverride) {
		if (asOverride) {
			m_AlbedoOverride = color;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetAlbedo(color);
		}
	}

	float MaterialInstance::GetMetallic() const {
		return m_MetallicOverride.value_or(m_BaseAsset->GetMetallic());
	}

	void MaterialInstance::SetMetallic(float metallic, bool asOverride) {
		if (asOverride) {
			m_MetallicOverride = metallic;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetMetallic(metallic);
		}
	}

	float MaterialInstance::GetRoughness() const {
		return m_RoughnessOverride.value_or(m_BaseAsset->GetRoughness());
	}

	void MaterialInstance::SetRoughness(float roughness, bool asOverride) {
		if (asOverride) {
			m_RoughnessOverride = roughness;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetRoughness(roughness);
		}
	}

	float MaterialInstance::GetSpecular() const {
		return m_SpecularOverride.value_or(m_BaseAsset->GetSpecular());
	}

	void MaterialInstance::SetSpecular(float specular, bool asOverride) {
		if (asOverride) {
			m_SpecularOverride = specular;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetSpecular(specular);
		}
	}

	glm::vec3 MaterialInstance::GetEmissionColor() const {
		return m_EmissionColorOverride.value_or(m_BaseAsset->GetEmissionColor());
	}

	void MaterialInstance::SetEmissionColor(const glm::vec3& color, bool asOverride) {
		if (asOverride) {
			m_EmissionColorOverride = color;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetEmissionColor(color);
		}
	}

	float MaterialInstance::GetEmissionIntensity() const {
		return m_EmissionIntensityOverride.value_or(m_BaseAsset->GetEmissionIntensity());
	}

	void MaterialInstance::SetEmissionIntensity(float intensity, bool asOverride) {
		if (asOverride) {
			m_EmissionIntensityOverride = intensity;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetEmissionIntensity(intensity);
		}
	}

	float MaterialInstance::GetNormalIntensity() const {
		return m_NormalIntensityOverride.value_or(m_BaseAsset->GetNormalIntensity());
	}

	void MaterialInstance::SetNormalIntensity(float intensity, bool asOverride) {
		if (asOverride) {
			m_NormalIntensityOverride = intensity;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetNormalIntensity(intensity);
		}
	}

	float MaterialInstance::GetMetallicMultiplier() const {
		return m_MetallicMultiplierOverride.value_or(m_BaseAsset->GetMetallicMultiplier());
	}

	void MaterialInstance::SetMetallicMultiplier(float multiplier, bool asOverride) {
		if (asOverride) {
			m_MetallicMultiplierOverride = multiplier;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetMetallicMultiplier(multiplier);
		}
	}

	float MaterialInstance::GetRoughnessMultiplier() const {
		return m_RoughnessMultiplierOverride.value_or(m_BaseAsset->GetRoughnessMultiplier());
	}

	void MaterialInstance::SetRoughnessMultiplier(float multiplier, bool asOverride) {
		if (asOverride) {
			m_RoughnessMultiplierOverride = multiplier;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetRoughnessMultiplier(multiplier);
		}
	}

	float MaterialInstance::GetSpecularMultiplier() const {
		return m_SpecularMultiplierOverride.value_or(m_BaseAsset->GetSpecularMultiplier());
	}

	void MaterialInstance::SetSpecularMultiplier(float multiplier, bool asOverride) {
		if (asOverride) {
			m_SpecularMultiplierOverride = multiplier;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetSpecularMultiplier(multiplier);
		}
	}

	float MaterialInstance::GetAOMultiplier() const {
		return m_AOMultiplierOverride.value_or(m_BaseAsset->GetAOMultiplier());
	}

	void MaterialInstance::SetAOMultiplier(float multiplier, bool asOverride) {
		if (asOverride) {
			m_AOMultiplierOverride = multiplier;
			MarkAsOverridden();
		} else {
			m_BaseAsset->SetAOMultiplier(multiplier);
		}
	}

	MaterialAsset::MaterialUniformData MaterialInstance::GetUniformData() const {
		MaterialAsset::MaterialUniformData data;
		memset(&data, 0, sizeof(data));

		data.Albedo = GetAlbedo();
		data.Metallic = GetMetallic();
		data.Roughness = GetRoughness();
		data.Specular = GetSpecular();
		data.EmissionIntensity = GetEmissionIntensity();
		data.EmissionColor = GetEmissionColor();
		data.NormalIntensity = GetNormalIntensity();

		data.UseAlbedoMap = HasAlbedoMap() ? 1 : 0;
		data.UseNormalMap = HasNormalMap() ? 1 : 0;
		data.UseMetallicMap = HasMetallicMap() ? 1 : 0;
		data.UseRoughnessMap = HasRoughnessMap() ? 1 : 0;
		data.UseSpecularMap = HasSpecularMap() ? 1 : 0;
		data.UseEmissionMap = HasEmissionMap() ? 1 : 0;
		data.UseAOMap = HasAOMap() ? 1 : 0;
		data._padding = 0.0f;

		data.MetallicMultiplier = GetMetallicMultiplier();
		data.RoughnessMultiplier = GetRoughnessMultiplier();
		data.SpecularMultiplier = GetSpecularMultiplier();
		data.AOMultiplier = GetAOMultiplier();

		// Detail normals (from base asset)
		const auto& details = GetDetailNormalMaps();
		data.DetailNormalCount = static_cast<int>(std::min(details.size(), (size_t)MaterialAsset::MAX_DETAIL_NORMALS));
		for (int i = 0; i < data.DetailNormalCount; ++i) {
			data.DetailNormalIntensities[i] = details[i].Intensity;
			data.DetailNormalTilingX[i] = details[i].TilingX;
			data.DetailNormalTilingY[i] = details[i].TilingY;
		}

		// Layered texture (from base asset)
		if (HasLayeredTexture()) {
			const auto& config = GetLayeredTextureConfig();
			data.UseLayeredTexture = 1;
			data.LayeredMetallicChannel = static_cast<int>(config.MetallicChannel);
			data.LayeredRoughnessChannel = static_cast<int>(config.RoughnessChannel);
			data.LayeredAOChannel = static_cast<int>(config.AOChannel);
			data.LayeredUseMetallic = config.UseForMetallic ? 1 : 0;
			data.LayeredUseRoughness = config.UseForRoughness ? 1 : 0;
			data.LayeredUseAO = config.UseForAO ? 1 : 0;
		} else {
			data.UseLayeredTexture = 0;
		}

		return data;
	}

	void MaterialInstance::BindTextures() const {
		// Bind or unbind each texture slot to prevent texture bleeding between draws
		if (auto albedoMap = GetAlbedoMap()) albedoMap->Bind(0);
		else glBindTextureUnit(0, 0);
		
		if (auto normalMap = GetNormalMap()) normalMap->Bind(1);
		else glBindTextureUnit(1, 0);
		
		if (auto metallicMap = GetMetallicMap()) metallicMap->Bind(2);
		else glBindTextureUnit(2, 0);
		
		if (auto roughnessMap = GetRoughnessMap()) roughnessMap->Bind(3);
		else glBindTextureUnit(3, 0);
		
		if (auto specularMap = GetSpecularMap()) specularMap->Bind(4);
		else glBindTextureUnit(4, 0);
		
		if (auto emissionMap = GetEmissionMap()) emissionMap->Bind(5);
		else glBindTextureUnit(5, 0);
		
		if (auto aoMap = GetAOMap()) aoMap->Bind(6);
		else glBindTextureUnit(6, 0);

		// Layered texture at slot 7
		if (HasLayeredTexture()) {
			auto layeredTex = GetLayeredTextureConfig().Texture;
			if (layeredTex) layeredTex->Bind(7);
			else glBindTextureUnit(7, 0);
		} else {
			glBindTextureUnit(7, 0);
		}

		// Detail normals at slots 12-15
		const auto& details = GetDetailNormalMaps();
		for (uint32_t i = 0; i < MaterialAsset::MAX_DETAIL_NORMALS; ++i) {
			if (i < details.size() && details[i].Texture) {
				details[i].Texture->Bind(12 + i);
			} else {
				glBindTextureUnit(12 + i, 0);
			}
		}
	}

} // namespace Lunex
