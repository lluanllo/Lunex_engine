#pragma once

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace Lunex {
	
	/**
	 * Material - Define propiedades visuales de una superficie
	 * Contiene el shader y parámetros por defecto
	 */
	class Material {
	public:
		Material(const Ref<Shader>& shader, const std::string& name = "Material");
		~Material() = default;

		void Bind();
		void Unbind();

		// Setters para propiedades comunes
		void SetAlbedo(const glm::vec3& albedo) { m_Albedo = albedo; }
		void SetMetallic(float metallic) { m_Metallic = metallic; }
		void SetRoughness(float roughness) { m_Roughness = roughness; }
		void SetEmission(const glm::vec3& emission) { m_Emission = emission; }

		void SetAlbedoMap(const Ref<Texture2D>& texture) { m_AlbedoMap = texture; }
		void SetNormalMap(const Ref<Texture2D>& texture) { m_NormalMap = texture; }
		void SetMetallicMap(const Ref<Texture2D>& texture) { m_MetallicMap = texture; }
		void SetRoughnessMap(const Ref<Texture2D>& texture) { m_RoughnessMap = texture; }

		// Getters
		const std::string& GetName() const { return m_Name; }
		Ref<Shader> GetShader() const { return m_Shader; }

		const glm::vec3& GetAlbedo() const { return m_Albedo; }
		float GetMetallic() const { return m_Metallic; }
		float GetRoughness() const { return m_Roughness; }
		const glm::vec3& GetEmission() const { return m_Emission; }

		Ref<Texture2D> GetAlbedoMap() const { return m_AlbedoMap; }
		Ref<Texture2D> GetNormalMap() const { return m_NormalMap; }
		Ref<Texture2D> GetMetallicMap() const { return m_MetallicMap; }
		Ref<Texture2D> GetRoughnessMap() const { return m_RoughnessMap; }

	private:
		std::string m_Name;
		Ref<Shader> m_Shader;

		// PBR properties
		glm::vec3 m_Albedo = { 1.0f, 1.0f, 1.0f };
		float m_Metallic = 0.0f;
		float m_Roughness = 0.5f;
		glm::vec3 m_Emission = { 0.0f, 0.0f, 0.0f };

		// Texture maps
		Ref<Texture2D> m_AlbedoMap;
		Ref<Texture2D> m_NormalMap;
		Ref<Texture2D> m_MetallicMap;
		Ref<Texture2D> m_RoughnessMap;

		friend class MaterialInstance;
	};

	/**
	 * MaterialInstance - Instancia de un material con overrides
	 * Permite tener múltiples objetos con el mismo material pero propiedades diferentes
	 */
	class MaterialInstance {
	public:
		MaterialInstance(const Ref<Material>& baseMaterial);
		~MaterialInstance() = default;

		void Bind();
		void Unbind();

		// Override properties
		void SetAlbedo(const glm::vec3& albedo);
		void SetMetallic(float metallic);
		void SetRoughness(float roughness);
		void SetEmission(const glm::vec3& emission);

		// Getters
		Ref<Material> GetBaseMaterial() const { return m_BaseMaterial; }

		glm::vec3 GetAlbedo() const {
			return m_OverrideAlbedo ? m_Albedo : m_BaseMaterial->GetAlbedo();
		}

		float GetMetallic() const {
			return m_OverrideMetallic ? m_Metallic : m_BaseMaterial->GetMetallic();
		}

		float GetRoughness() const {
			return m_OverrideRoughness ? m_Roughness : m_BaseMaterial->GetRoughness();
		}

		glm::vec3 GetEmission() const {
			return m_OverrideEmission ? m_Emission : m_BaseMaterial->GetEmission();
		}

	private:
		Ref<Material> m_BaseMaterial;

		// Overridden properties (optional)
		bool m_OverrideAlbedo = false;
		glm::vec3 m_Albedo;

		bool m_OverrideMetallic = false;
		float m_Metallic;

		bool m_OverrideRoughness = false;
		float m_Roughness;

		bool m_OverrideEmission = false;
		glm::vec3 m_Emission;
	};

}
