#include "stpch.h"
#include "MaterialInstance.h"

namespace Lunex {

	// ============================================================================
	// Material Implementation
	// ============================================================================

	Material::Material(const Ref<Shader>& shader, const std::string& name)
		: m_Shader(shader), m_Name(name) {
	}

	void Material::Bind() {
		if (!m_Shader) {
			LNX_LOG_ERROR("Material::Bind() - No shader assigned!");
			return;
		}

		m_Shader->Bind();

		// Upload material properties to UBO (binding = 2)
		struct MaterialUBO {
			glm::vec4 Albedo_Metallic;           // xyz = Albedo, w = Metallic
			glm::vec4 Roughness_Emission_X;      // x = Roughness, yzw = Emission
			glm::vec4 Flags;             // xyzw = HasAlbedoMap, HasNormalMap, HasMetallicMap, HasRoughnessMap
		} materialUbo;

		materialUbo.Albedo_Metallic = glm::vec4(m_Albedo, m_Metallic);
		materialUbo.Roughness_Emission_X = glm::vec4(m_Roughness, m_Emission.x, m_Emission.y, m_Emission.z);
		materialUbo.Flags = glm::vec4(
			m_AlbedoMap ? 1.0f : 0.0f,
			m_NormalMap ? 1.0f : 0.0f,
			m_MetallicMap ? 1.0f : 0.0f,
			m_RoughnessMap ? 1.0f : 0.0f
		);

		// TODO: Upload to UBO (binding = 2) instead of individual uniforms
		// For now, use SetFloat4() which should work with UBO if the layout matches
		m_Shader->SetFloat4("u_Material_Albedo_Metallic", materialUbo.Albedo_Metallic);
		m_Shader->SetFloat4("u_Material_Roughness_Emission_X", materialUbo.Roughness_Emission_X);
		m_Shader->SetFloat4("u_Material_Flags", materialUbo.Flags);

		// Debug log (solo la primera vez)
		static bool logged = false;
		if (!logged) {
			LNX_LOG_INFO("Material::Bind() - Albedo: ({0}, {1}, {2}), Metallic: {3}, Roughness: {4}",
				m_Albedo.x, m_Albedo.y, m_Albedo.z, m_Metallic, m_Roughness);
			logged = true;
		}

		// Bind textures to explicit binding points (matching shader layout(binding = X))
		if (m_AlbedoMap) {
			m_AlbedoMap->Bind(0); // binding = 0
		}

		if (m_NormalMap) {
			m_NormalMap->Bind(1); // binding = 1
		}

		if (m_MetallicMap) {
			m_MetallicMap->Bind(2); // binding = 2
		}

		if (m_RoughnessMap) {
			m_RoughnessMap->Bind(3); // binding = 3
		}
	}

	void Material::Unbind() {
		if (m_Shader) {
			m_Shader->Unbind();
		}
	}

	// ============================================================================
	// MaterialInstance Implementation
	// ============================================================================

	MaterialInstance::MaterialInstance(const Ref<Material>& baseMaterial)
		: m_BaseMaterial(baseMaterial) {
	}

	void MaterialInstance::Bind() {
		if (!m_BaseMaterial) return;

		// Bind base material first
		m_BaseMaterial->Bind();

		// Override properties if set
		Ref<Shader> shader = m_BaseMaterial->GetShader();
		if (!shader) return;

		// Override individual properties (rebuild the UBO data)
		if (m_OverrideAlbedo || m_OverrideMetallic) {
			glm::vec3 albedo = m_OverrideAlbedo ? m_Albedo : m_BaseMaterial->GetAlbedo();
			float metallic = m_OverrideMetallic ? m_Metallic : m_BaseMaterial->GetMetallic();
			shader->SetFloat4("u_Material_Albedo_Metallic", glm::vec4(albedo, metallic));
		}

		if (m_OverrideRoughness || m_OverrideEmission) {
			float roughness = m_OverrideRoughness ? m_Roughness : m_BaseMaterial->GetRoughness();
			glm::vec3 emission = m_OverrideEmission ? m_Emission : m_BaseMaterial->GetEmission();
			shader->SetFloat4("u_Material_Roughness_Emission_X", glm::vec4(roughness, emission.x, emission.y, emission.z));
		}
	}

	void MaterialInstance::Unbind() {
		if (m_BaseMaterial) {
			m_BaseMaterial->Unbind();
		}
	}

	void MaterialInstance::SetAlbedo(const glm::vec3& albedo) {
		m_Albedo = albedo;
		m_OverrideAlbedo = true;
	}

	void MaterialInstance::SetMetallic(float metallic) {
		m_Metallic = metallic;
		m_OverrideMetallic = true;
	}

	void MaterialInstance::SetRoughness(float roughness) {
		m_Roughness = roughness;
		m_OverrideRoughness = true;
	}

	void MaterialInstance::SetEmission(const glm::vec3& emission) {
		m_Emission = emission;
		m_OverrideEmission = true;
	}

}