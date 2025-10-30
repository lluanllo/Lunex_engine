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

		// ? NO usar SetFloat4() - estos van como uniforms individuales, NO como UBO
		// El UBO se sube desde RendererPipeline3D::Flush()
		
		// ? Solo bind textures aquí
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