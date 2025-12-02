#pragma once

#include "Core/Core.h"
#include "Core/UUID.h"
#include "Texture.h"
#include <glm/glm.hpp>
#include <string>
#include <filesystem>

// Forward declarations para evitar incluir yaml-cpp en el header
namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex {

	// MaterialAsset - Recurso serializable que representa un material físicamente basado (PBR)
	// Puede ser compartido entre múltiples entidades y guardado como archivo .lumat
	class MaterialAsset {
	public:
		MaterialAsset();
		MaterialAsset(const std::string& name);
		~MaterialAsset() = default;

		// ========== IDENTIFICACIÓN ==========
		UUID GetID() const { return m_ID; }
		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		const std::filesystem::path& GetPath() const { return m_FilePath; }
		void SetPath(const std::filesystem::path& path) { m_FilePath = path; }

		// ========== PROPIEDADES PBR ==========
		
		// Albedo (Color base)
		void SetAlbedo(const glm::vec4& color) { m_Albedo = color; m_IsDirty = true; }
		const glm::vec4& GetAlbedo() const { return m_Albedo; }

		// Metallic (0 = dieléctrico, 1 = metal)
		void SetMetallic(float metallic) { m_Metallic = glm::clamp(metallic, 0.0f, 1.0f); m_IsDirty = true; }
		float GetMetallic() const { return m_Metallic; }

		// Roughness (0 = liso, 1 = rugoso)
		void SetRoughness(float roughness) { m_Roughness = glm::clamp(roughness, 0.0f, 1.0f); m_IsDirty = true; }
		float GetRoughness() const { return m_Roughness; }

		// Specular (reflectividad en ángulo normal)
		void SetSpecular(float specular) { m_Specular = glm::clamp(specular, 0.0f, 1.0f); m_IsDirty = true; }
		float GetSpecular() const { return m_Specular; }

		// Emission (emisión de luz)
		void SetEmissionColor(const glm::vec3& color) { m_EmissionColor = color; m_IsDirty = true; }
		const glm::vec3& GetEmissionColor() const { return m_EmissionColor; }

		void SetEmissionIntensity(float intensity) { m_EmissionIntensity = glm::max(0.0f, intensity); m_IsDirty = true; }
		float GetEmissionIntensity() const { return m_EmissionIntensity; }

		// Normal intensity (para controlar la intensidad del normal map)
		void SetNormalIntensity(float intensity) { m_NormalIntensity = glm::clamp(intensity, 0.0f, 2.0f); m_IsDirty = true; }
		float GetNormalIntensity() const { return m_NormalIntensity; }

		// ========== TEXTURAS PBR ==========
		
		// Albedo Map
		void SetAlbedoMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetAlbedoMap() const { return m_AlbedoMap; }
		const std::string& GetAlbedoPath() const { return m_AlbedoPath; }
		bool HasAlbedoMap() const { return m_AlbedoMap != nullptr; }

		// Normal Map
		void SetNormalMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetNormalMap() const { return m_NormalMap; }
		const std::string& GetNormalPath() const { return m_NormalPath; }
		bool HasNormalMap() const { return m_NormalMap != nullptr; }

		// Metallic Map
		void SetMetallicMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetMetallicMap() const { return m_MetallicMap; }
		const std::string& GetMetallicPath() const { return m_MetallicPath; }
		bool HasMetallicMap() const { return m_MetallicMap != nullptr; }
		void SetMetallicMultiplier(float multiplier) { m_MetallicMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); m_IsDirty = true; }
		float GetMetallicMultiplier() const { return m_MetallicMultiplier; }

		// Roughness Map
		void SetRoughnessMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetRoughnessMap() const { return m_RoughnessMap; }
		const std::string& GetRoughnessPath() const { return m_RoughnessPath; }
		bool HasRoughnessMap() const { return m_RoughnessMap != nullptr; }
		void SetRoughnessMultiplier(float multiplier) { m_RoughnessMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); m_IsDirty = true; }
		float GetRoughnessMultiplier() const { return m_RoughnessMultiplier; }

		// Specular Map
		void SetSpecularMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetSpecularMap() const { return m_SpecularMap; }
		const std::string& GetSpecularPath() const { return m_SpecularPath; }
		bool HasSpecularMap() const { return m_SpecularMap != nullptr; }
		void SetSpecularMultiplier(float multiplier) { m_SpecularMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); m_IsDirty = true; }
		float GetSpecularMultiplier() const { return m_SpecularMultiplier; }

		// Emission Map
		void SetEmissionMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetEmissionMap() const { return m_EmissionMap; }
		const std::string& GetEmissionPath() const { return m_EmissionPath; }
		bool HasEmissionMap() const { return m_EmissionMap != nullptr; }

		// Ambient Occlusion Map
		void SetAOMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetAOMap() const { return m_AOMap; }
		const std::string& GetAOPath() const { return m_AOPath; }
		bool HasAOMap() const { return m_AOMap != nullptr; }
		void SetAOMultiplier(float multiplier) { m_AOMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); m_IsDirty = true; }
		float GetAOMultiplier() const { return m_AOMultiplier; }

		// ========== SERIALIZACIÓN ==========
		
		// Guardar material a archivo .lumat
		bool SaveToFile(const std::filesystem::path& path);
		bool SaveToFile(); // Usa m_FilePath
		
		// Cargar material desde archivo .lumat
		static Ref<MaterialAsset> LoadFromFile(const std::filesystem::path& path);

		// ========== UTILIDADES ==========
		
		// Verificar si hay cambios sin guardar
		bool IsDirty() const { return m_IsDirty; }
		void SetDirty(bool dirty) { m_IsDirty = dirty; }

		// Verificar si tiene alguna textura
		bool HasAnyTexture() const {
			return HasAlbedoMap() || HasNormalMap() || HasMetallicMap() || 
				   HasRoughnessMap() || HasSpecularMap() || HasEmissionMap() || HasAOMap();
		}

		// Crear copia del material (deep copy)
		Ref<MaterialAsset> Clone() const;

		// ========== DATOS PARA SHADER ==========
		
		// Estructura para upload a GPU (alineada para UBO)
		struct MaterialUniformData {
			glm::vec4 Albedo;
			float Metallic;
			float Roughness;
			float Specular;
			float EmissionIntensity;
			glm::vec3 EmissionColor;
			float NormalIntensity;

			// Texture flags
			int UseAlbedoMap;
			int UseNormalMap;
			int UseMetallicMap;
			int UseRoughnessMap;
			int UseSpecularMap;
			int UseEmissionMap;
			int UseAOMap;
			float _padding;

			// Texture multipliers
			float MetallicMultiplier;
			float RoughnessMultiplier;
			float SpecularMultiplier;
			float AOMultiplier;
		};

		MaterialUniformData GetUniformData() const;

	private:
		// Identificación
		UUID m_ID;
		std::string m_Name = "New Material";
		std::filesystem::path m_FilePath;

		// Propiedades PBR
		glm::vec4 m_Albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		float m_Metallic = 0.0f;
		float m_Roughness = 0.5f;
		float m_Specular = 0.5f;
		glm::vec3 m_EmissionColor = { 0.0f, 0.0f, 0.0f };
		float m_EmissionIntensity = 0.0f;
		float m_NormalIntensity = 1.0f;

		// Texturas PBR
		Ref<Texture2D> m_AlbedoMap;
		std::string m_AlbedoPath;

		Ref<Texture2D> m_NormalMap;
		std::string m_NormalPath;

		Ref<Texture2D> m_MetallicMap;
		std::string m_MetallicPath;
		float m_MetallicMultiplier = 1.0f;

		Ref<Texture2D> m_RoughnessMap;
		std::string m_RoughnessPath;
		float m_RoughnessMultiplier = 1.0f;

		Ref<Texture2D> m_SpecularMap;
		std::string m_SpecularPath;
		float m_SpecularMultiplier = 1.0f;

		Ref<Texture2D> m_EmissionMap;
		std::string m_EmissionPath;

		Ref<Texture2D> m_AOMap;
		std::string m_AOPath;
		float m_AOMultiplier = 1.0f;

		// Estado
		bool m_IsDirty = false; // Cambios sin guardar

		// Helpers privados para serialización
		void SerializeTexture(YAML::Emitter& out, const std::string& key, const std::string& path) const;
		void DeserializeTexture(const YAML::Node& node, const std::string& key, 
								Ref<Texture2D>& texture, std::string& path);
	};

}
