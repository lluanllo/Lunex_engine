#include "stpch.h"
#include "MaterialAsset.h"
#include "Log/Log.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Lunex {

	MaterialAsset::MaterialAsset()
		: m_ID(UUID())
	{
	}

	MaterialAsset::MaterialAsset(const std::string& name)
		: m_ID(UUID()), m_Name(name)
	{
	}

	// ========== TEXTURAS ==========

	void MaterialAsset::SetAlbedoMap(Ref<Texture2D> texture) {
		m_AlbedoMap = texture;
		if (texture && texture->IsLoaded()) {
			m_AlbedoPath = texture->GetPath();
		}
		m_IsDirty = true;
	}

	void MaterialAsset::SetNormalMap(Ref<Texture2D> texture) {
		m_NormalMap = texture;
		if (texture && texture->IsLoaded()) {
			m_NormalPath = texture->GetPath();
		}
		m_IsDirty = true;
	}

	void MaterialAsset::SetMetallicMap(Ref<Texture2D> texture) {
		m_MetallicMap = texture;
		if (texture && texture->IsLoaded()) {
			m_MetallicPath = texture->GetPath();
		}
		m_IsDirty = true;
	}

	void MaterialAsset::SetRoughnessMap(Ref<Texture2D> texture) {
		m_RoughnessMap = texture;
		if (texture && texture->IsLoaded()) {
			m_RoughnessPath = texture->GetPath();
		}
		m_IsDirty = true;
	}

	void MaterialAsset::SetSpecularMap(Ref<Texture2D> texture) {
		m_SpecularMap = texture;
		if (texture && texture->IsLoaded()) {
			m_SpecularPath = texture->GetPath();
		}
		m_IsDirty = true;
	}

	void MaterialAsset::SetEmissionMap(Ref<Texture2D> texture) {
		m_EmissionMap = texture;
		if (texture && texture->IsLoaded()) {
			m_EmissionPath = texture->GetPath();
		}
		m_IsDirty = true;
	}

	void MaterialAsset::SetAOMap(Ref<Texture2D> texture) {
		m_AOMap = texture;
		if (texture && texture->IsLoaded()) {
			m_AOPath = texture->GetPath();
		}
		m_IsDirty = true;
	}

	// ========== SERIALIZACIÓN ==========

	bool MaterialAsset::SaveToFile(const std::filesystem::path& path) {
		m_FilePath = path;
		return SaveToFile();
	}

	bool MaterialAsset::SaveToFile() {
		if (m_FilePath.empty()) {
			LNX_LOG_ERROR("MaterialAsset::SaveToFile - No file path specified");
			return false;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;

		// Metadata
		out << YAML::Key << "Material" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "ID" << YAML::Value << (uint64_t)m_ID;
		out << YAML::Key << "Name" << YAML::Value << m_Name;
		out << YAML::EndMap;

		// PBR Properties
		out << YAML::Key << "Properties" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Albedo" << YAML::Value << YAML::Flow << YAML::BeginSeq 
			<< m_Albedo.r << m_Albedo.g << m_Albedo.b << m_Albedo.a << YAML::EndSeq;
		out << YAML::Key << "Metallic" << YAML::Value << m_Metallic;
		out << YAML::Key << "Roughness" << YAML::Value << m_Roughness;
		out << YAML::Key << "Specular" << YAML::Value << m_Specular;
		out << YAML::Key << "EmissionColor" << YAML::Value << YAML::Flow << YAML::BeginSeq 
			<< m_EmissionColor.r << m_EmissionColor.g << m_EmissionColor.b << YAML::EndSeq;
		out << YAML::Key << "EmissionIntensity" << YAML::Value << m_EmissionIntensity;
		out << YAML::Key << "NormalIntensity" << YAML::Value << m_NormalIntensity;
		out << YAML::EndMap;

		// Textures
		out << YAML::Key << "Textures" << YAML::Value;
		out << YAML::BeginMap;
		SerializeTexture(out, "Albedo", m_AlbedoPath);
		SerializeTexture(out, "Normal", m_NormalPath);
		SerializeTexture(out, "Metallic", m_MetallicPath);
		SerializeTexture(out, "Roughness", m_RoughnessPath);
		SerializeTexture(out, "Specular", m_SpecularPath);
		SerializeTexture(out, "Emission", m_EmissionPath);
		SerializeTexture(out, "AO", m_AOPath);
		out << YAML::EndMap;

		// Texture Multipliers
		out << YAML::Key << "Multipliers" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Metallic" << YAML::Value << m_MetallicMultiplier;
		out << YAML::Key << "Roughness" << YAML::Value << m_RoughnessMultiplier;
		out << YAML::Key << "Specular" << YAML::Value << m_SpecularMultiplier;
		out << YAML::Key << "AO" << YAML::Value << m_AOMultiplier;
		out << YAML::EndMap;

		out << YAML::EndMap;

		// Write to file
		std::ofstream fout(m_FilePath);
		if (!fout.is_open()) {
			LNX_LOG_ERROR("MaterialAsset::SaveToFile - Failed to open file: {0}", m_FilePath.string());
			return false;
		}

		fout << out.c_str();
		fout.close();

		m_IsDirty = false;
		LNX_LOG_INFO("Material saved: {0}", m_FilePath.string());
		return true;
	}

	Ref<MaterialAsset> MaterialAsset::LoadFromFile(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("MaterialAsset::LoadFromFile - File not found: {0}", path.string());
			return nullptr;
		}

		std::ifstream stream(path);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data;
		try {
			data = YAML::Load(strStream.str());
		}
		catch (YAML::ParserException e) {
			LNX_LOG_ERROR("MaterialAsset::LoadFromFile - Failed to parse YAML: {0}", e.what());
			return nullptr;
		}

		auto materialNode = data["Material"];
		if (!materialNode) {
			LNX_LOG_ERROR("MaterialAsset::LoadFromFile - Invalid material file format");
			return nullptr;
		}

		Ref<MaterialAsset> material = CreateRef<MaterialAsset>();
		material->m_FilePath = path;

		// Metadata
		material->m_ID = UUID(materialNode["ID"].as<uint64_t>());
		material->m_Name = materialNode["Name"].as<std::string>();

		// Properties
		auto propsNode = data["Properties"];
		if (propsNode) {
			auto albedo = propsNode["Albedo"];
			if (albedo) {
				material->m_Albedo = glm::vec4(
					albedo[0].as<float>(),
					albedo[1].as<float>(),
					albedo[2].as<float>(),
					albedo[3].as<float>()
				);
			}

			material->m_Metallic = propsNode["Metallic"].as<float>(0.0f);
			material->m_Roughness = propsNode["Roughness"].as<float>(0.5f);
			material->m_Specular = propsNode["Specular"].as<float>(0.5f);

			auto emission = propsNode["EmissionColor"];
			if (emission) {
				material->m_EmissionColor = glm::vec3(
					emission[0].as<float>(),
					emission[1].as<float>(),
					emission[2].as<float>()
				);
			}

			material->m_EmissionIntensity = propsNode["EmissionIntensity"].as<float>(0.0f);
			material->m_NormalIntensity = propsNode["NormalIntensity"].as<float>(1.0f);
		}

		// Textures
		auto texturesNode = data["Textures"];
		if (texturesNode) {
			material->DeserializeTexture(texturesNode, "Albedo", material->m_AlbedoMap, material->m_AlbedoPath);
			material->DeserializeTexture(texturesNode, "Normal", material->m_NormalMap, material->m_NormalPath);
			material->DeserializeTexture(texturesNode, "Metallic", material->m_MetallicMap, material->m_MetallicPath);
			material->DeserializeTexture(texturesNode, "Roughness", material->m_RoughnessMap, material->m_RoughnessPath);
			material->DeserializeTexture(texturesNode, "Specular", material->m_SpecularMap, material->m_SpecularPath);
			material->DeserializeTexture(texturesNode, "Emission", material->m_EmissionMap, material->m_EmissionPath);
			material->DeserializeTexture(texturesNode, "AO", material->m_AOMap, material->m_AOPath);
		}

		// Multipliers
		auto multipliersNode = data["Multipliers"];
		if (multipliersNode) {
			material->m_MetallicMultiplier = multipliersNode["Metallic"].as<float>(1.0f);
			material->m_RoughnessMultiplier = multipliersNode["Roughness"].as<float>(1.0f);
			material->m_SpecularMultiplier = multipliersNode["Specular"].as<float>(1.0f);
			material->m_AOMultiplier = multipliersNode["AO"].as<float>(1.0f);
		}

		material->m_IsDirty = false;
		LNX_LOG_INFO("Material loaded: {0}", path.string());
		return material;
	}

	// ========== UTILIDADES ==========

	Ref<MaterialAsset> MaterialAsset::Clone() const {
		Ref<MaterialAsset> clone = CreateRef<MaterialAsset>(m_Name + " (Clone)");

		// Copy properties
		clone->m_Albedo = m_Albedo;
		clone->m_Metallic = m_Metallic;
		clone->m_Roughness = m_Roughness;
		clone->m_Specular = m_Specular;
		clone->m_EmissionColor = m_EmissionColor;
		clone->m_EmissionIntensity = m_EmissionIntensity;
		clone->m_NormalIntensity = m_NormalIntensity;

		// Copy textures (shared references, not deep copy of texture data)
		clone->m_AlbedoMap = m_AlbedoMap;
		clone->m_AlbedoPath = m_AlbedoPath;

		clone->m_NormalMap = m_NormalMap;
		clone->m_NormalPath = m_NormalPath;

		clone->m_MetallicMap = m_MetallicMap;
		clone->m_MetallicPath = m_MetallicPath;
		clone->m_MetallicMultiplier = m_MetallicMultiplier;

		clone->m_RoughnessMap = m_RoughnessMap;
		clone->m_RoughnessPath = m_RoughnessPath;
		clone->m_RoughnessMultiplier = m_RoughnessMultiplier;

		clone->m_SpecularMap = m_SpecularMap;
		clone->m_SpecularPath = m_SpecularPath;
		clone->m_SpecularMultiplier = m_SpecularMultiplier;

		clone->m_EmissionMap = m_EmissionMap;
		clone->m_EmissionPath = m_EmissionPath;

		clone->m_AOMap = m_AOMap;
		clone->m_AOPath = m_AOPath;
		clone->m_AOMultiplier = m_AOMultiplier;

		clone->m_IsDirty = true;
		return clone;
	}

	MaterialAsset::MaterialUniformData MaterialAsset::GetUniformData() const {
		MaterialUniformData data;

		data.Albedo = m_Albedo;
		data.Metallic = m_Metallic;
		data.Roughness = m_Roughness;
		data.Specular = m_Specular;
		data.EmissionIntensity = m_EmissionIntensity;
		data.EmissionColor = m_EmissionColor;
		data.NormalIntensity = m_NormalIntensity;

		data.UseAlbedoMap = HasAlbedoMap() ? 1 : 0;
		data.UseNormalMap = HasNormalMap() ? 1 : 0;
		data.UseMetallicMap = HasMetallicMap() ? 1 : 0;
		data.UseRoughnessMap = HasRoughnessMap() ? 1 : 0;
		data.UseSpecularMap = HasSpecularMap() ? 1 : 0;
		data.UseEmissionMap = HasEmissionMap() ? 1 : 0;
		data.UseAOMap = HasAOMap() ? 1 : 0;
		data._padding = 0.0f;

		data.MetallicMultiplier = m_MetallicMultiplier;
		data.RoughnessMultiplier = m_RoughnessMultiplier;
		data.SpecularMultiplier = m_SpecularMultiplier;
		data.AOMultiplier = m_AOMultiplier;

		return data;
	}

	// ========== HELPERS PRIVADOS ==========

	void MaterialAsset::SerializeTexture(YAML::Emitter& out, const std::string& key, const std::string& path) const {
		if (!path.empty()) {
			out << YAML::Key << key << YAML::Value << path;
		}
	}

	void MaterialAsset::DeserializeTexture(const YAML::Node& node, const std::string& key,
										   Ref<Texture2D>& texture, std::string& path) {
		if (node[key]) {
			path = node[key].as<std::string>();
			if (!path.empty() && std::filesystem::exists(path)) {
				texture = Texture2D::Create(path);
				if (!texture || !texture->IsLoaded()) {
					LNX_LOG_WARN("Failed to load texture: {0}", path);
					texture = nullptr;
					path.clear();
				}
			}
		}
	}

}
