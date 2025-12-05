#pragma once

#include "Core/Core.h"
#include "MaterialAsset.h"
#include <memory>

namespace Lunex {

	// MaterialInstance - Instancia de un MaterialAsset que puede ser compartida
	// Implementa reference counting y permite overrides locales sin modificar el asset original
	// Inspirado en el sistema de instancias de materiales de Unity/Unreal
	class MaterialInstance {
	public:
		// Crear instancia desde un MaterialAsset
		MaterialInstance(Ref<MaterialAsset> baseAsset);
		
		// Crear instancia desde un archivo .umat
		static Ref<MaterialInstance> Create(const std::filesystem::path& assetPath);
		
		// Crear instancia desde un MaterialAsset existente
		static Ref<MaterialInstance> Create(Ref<MaterialAsset> baseAsset);
		
		// Clone this instance (preserves all overrides)
		Ref<MaterialInstance> Clone() const;

		~MaterialInstance() = default;

		// ========== ASSET BASE ==========
		
		Ref<MaterialAsset> GetBaseAsset() const { return m_BaseAsset; }
		
		// Reemplazar el asset base (perderá overrides locales)
		void SetBaseAsset(Ref<MaterialAsset> asset);
		
		// Verificar si hay overrides locales
		bool HasLocalOverrides() const { return m_HasLocalOverrides; }
		
		// Resetear overrides (volver al asset base)
		void ResetOverrides();

		// ========== PROPIEDADES PBR (con override support) ==========
		
		// Si hay override local, devuelve el override; si no, devuelve del asset base
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

		// ========== TEXTURAS (siempre desde base asset) ==========
		// Las texturas NO se pueden hacer override por instancia (policy decision)
		// Para diferentes texturas, crear un nuevo MaterialAsset
		
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

		// Multipliers (pueden ser override)
		float GetMetallicMultiplier() const;
		void SetMetallicMultiplier(float multiplier, bool asOverride = false);

		float GetRoughnessMultiplier() const;
		void SetRoughnessMultiplier(float multiplier, bool asOverride = false);

		float GetSpecularMultiplier() const;
		void SetSpecularMultiplier(float multiplier, bool asOverride = false);

		float GetAOMultiplier() const;
		void SetAOMultiplier(float multiplier, bool asOverride = false);

		// ========== DATOS PARA RENDERING ==========
		
		// Obtener datos finales para el shader (con overrides aplicados)
		MaterialAsset::MaterialUniformData GetUniformData() const;

		// Bind de texturas a slots específicos
		void BindTextures() const;

		// ========== INFORMACIÓN ==========
		
		const std::string& GetName() const { return m_BaseAsset->GetName(); }
		UUID GetAssetID() const { return m_BaseAsset->GetID(); }
		const std::filesystem::path& GetAssetPath() const { return m_BaseAsset->GetPath(); }

	private:
		// Asset base (shared)
		Ref<MaterialAsset> m_BaseAsset;

		// ========== OVERRIDES LOCALES ==========
		// Solo se usan si m_HasLocalOverrides es true
		// Esto permite instancias ligeras que solo almacenan cambios locales
		
		bool m_HasLocalOverrides = false;

		// Property overrides (std::optional para saber qué está overridden)
		std::optional<glm::vec4> m_AlbedoOverride;
		std::optional<float> m_MetallicOverride;
		std::optional<float> m_RoughnessOverride;
		std::optional<float> m_SpecularOverride;
		std::optional<glm::vec3> m_EmissionColorOverride;
		std::optional<float> m_EmissionIntensityOverride;
		std::optional<float> m_NormalIntensityOverride;

		// Multiplier overrides
		std::optional<float> m_MetallicMultiplierOverride;
		std::optional<float> m_RoughnessMultiplierOverride;
		std::optional<float> m_SpecularMultiplierOverride;
		std::optional<float> m_AOMultiplierOverride;

		// Helper para marcar que hay overrides
		void MarkAsOverridden() { m_HasLocalOverrides = true; }
	};

}
