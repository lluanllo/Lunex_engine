#include "stpch.h"
#include "MaterialAsset.h"
#include "Log/Log.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Lunex {

	MaterialAsset::MaterialAsset()
		: Asset()
	{
		m_Name = "New Material";
	}

	MaterialAsset::MaterialAsset(const std::string& name)
		: Asset(name)
	{
	}

	// ========== TEXTURES ==========

	void MaterialAsset::SetAlbedoMap(Ref<Texture2D> texture) {
		m_AlbedoMap = texture;
		if (texture && texture->IsLoaded()) {
			m_AlbedoPath = texture->GetPath();
		} else {
			m_AlbedoPath.clear();
		}
		MarkDirty();
	}

	void MaterialAsset::SetNormalMap(Ref<Texture2D> texture) {
		m_NormalMap = texture;
		if (texture && texture->IsLoaded()) {
			m_NormalPath = texture->GetPath();
		} else {
			m_NormalPath.clear();
		}
		MarkDirty();
	}

	void MaterialAsset::SetMetallicMap(Ref<Texture2D> texture) {
		m_MetallicMap = texture;
		if (texture && texture->IsLoaded()) {
			m_MetallicPath = texture->GetPath();
		} else {
			m_MetallicPath.clear();
		}
		MarkDirty();
	}

	void MaterialAsset::SetRoughnessMap(Ref<Texture2D> texture) {
		m_RoughnessMap = texture;
		if (texture && texture->IsLoaded()) {
			m_RoughnessPath = texture->GetPath();
		} else {
			m_RoughnessPath.clear();
		}
		MarkDirty();
	}

	void MaterialAsset::SetSpecularMap(Ref<Texture2D> texture) {
		m_SpecularMap = texture;
		if (texture && texture->IsLoaded()) {
			m_SpecularPath = texture->GetPath();
		} else {
			m_SpecularPath.clear();
		}
		MarkDirty();
	}

	void MaterialAsset::SetEmissionMap(Ref<Texture2D> texture) {
		m_EmissionMap = texture;
		if (texture && texture->IsLoaded()) {
			m_EmissionPath = texture->GetPath();
		} else {
			m_EmissionPath.clear();
		}
		MarkDirty();
	}

	void MaterialAsset::SetAOMap(Ref<Texture2D> texture) {
		m_AOMap = texture;
		if (texture && texture->IsLoaded()) {
			m_AOPath = texture->GetPath();
		} else {
			m_AOPath.clear();
		}
		MarkDirty();
	}

	// ========== DETAIL NORMAL MAPS ==========

	void MaterialAsset::AddDetailNormalMap(const DetailNormalMap& detail) {
		m_DetailNormalMaps.push_back(detail);
		MarkDirty();
	}

	void MaterialAsset::RemoveDetailNormalMap(size_t index) {
		if (index < m_DetailNormalMaps.size()) {
			m_DetailNormalMaps.erase(m_DetailNormalMaps.begin() + index);
			MarkDirty();
		}
	}

	void MaterialAsset::SetDetailNormalMap(size_t index, const DetailNormalMap& detail) {
		if (index < m_DetailNormalMaps.size()) {
			m_DetailNormalMaps[index] = detail;
			MarkDirty();
		}
	}

	// ========== LAYERED TEXTURE ==========

	void MaterialAsset::SetLayeredTexture(Ref<Texture2D> texture) {
		m_LayeredConfig.Texture = texture;
		if (texture && texture->IsLoaded()) {
			m_LayeredConfig.Path = texture->GetPath();
			m_LayeredConfig.Enabled = true;
		} else {
			m_LayeredConfig.Path.clear();
			m_LayeredConfig.Enabled = false;
		}
		MarkDirty();
	}

	// ========== SERIALIZATION ==========

	bool MaterialAsset::SaveToFile(const std::filesystem::path& path) {
		m_FilePath = path;

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

		// Color spaces
		out << YAML::Key << "ColorSpaces" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Albedo" << YAML::Value << static_cast<int>(m_AlbedoColorSpace);
		out << YAML::Key << "Emission" << YAML::Value << static_cast<int>(m_EmissionColorSpace);
		out << YAML::Key << "Normal" << YAML::Value << static_cast<int>(m_NormalColorSpace);
		out << YAML::Key << "Metallic" << YAML::Value << static_cast<int>(m_MetallicColorSpace);
		out << YAML::Key << "Roughness" << YAML::Value << static_cast<int>(m_RoughnessColorSpace);
		out << YAML::Key << "AO" << YAML::Value << static_cast<int>(m_AOColorSpace);
		out << YAML::EndMap;

		// Alpha packing
		out << YAML::Key << "AlphaPacking" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Enabled" << YAML::Value << m_AlphaIsPacked;
		out << YAML::Key << "Channel" << YAML::Value << static_cast<int>(m_AlphaPackedChannel);
		out << YAML::EndMap;

		// Detail normal maps
		if (!m_DetailNormalMaps.empty()) {
			out << YAML::Key << "DetailNormals" << YAML::Value;
			out << YAML::BeginSeq;
			for (const auto& detail : m_DetailNormalMaps) {
				out << YAML::BeginMap;
				out << YAML::Key << "Path" << YAML::Value << detail.Path;
				out << YAML::Key << "Intensity" << YAML::Value << detail.Intensity;
				out << YAML::Key << "TilingX" << YAML::Value << detail.TilingX;
				out << YAML::Key << "TilingY" << YAML::Value << detail.TilingY;
				out << YAML::Key << "OffsetX" << YAML::Value << detail.OffsetX;
				out << YAML::Key << "OffsetY" << YAML::Value << detail.OffsetY;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}

		// Layered texture
		if (m_LayeredConfig.Enabled) {
			out << YAML::Key << "LayeredTexture" << YAML::Value;
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << m_LayeredConfig.Path;
			out << YAML::Key << "MetallicChannel" << YAML::Value << static_cast<int>(m_LayeredConfig.MetallicChannel);
			out << YAML::Key << "RoughnessChannel" << YAML::Value << static_cast<int>(m_LayeredConfig.RoughnessChannel);
			out << YAML::Key << "AOChannel" << YAML::Value << static_cast<int>(m_LayeredConfig.AOChannel);
			out << YAML::Key << "UseForMetallic" << YAML::Value << m_LayeredConfig.UseForMetallic;
			out << YAML::Key << "UseForRoughness" << YAML::Value << m_LayeredConfig.UseForRoughness;
			out << YAML::Key << "UseForAO" << YAML::Value << m_LayeredConfig.UseForAO;
			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		std::ofstream fout(m_FilePath);
		if (!fout.is_open()) {
			LNX_LOG_ERROR("MaterialAsset::SaveToFile - Failed to open file: {0}", m_FilePath.string());
			return false;
		}

		fout << out.c_str();
		fout.close();

		ClearDirty();
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

		// Color spaces
		auto csNode = data["ColorSpaces"];
		if (csNode) {
			material->m_AlbedoColorSpace = static_cast<TextureColorSpace>(csNode["Albedo"].as<int>(0));
			material->m_EmissionColorSpace = static_cast<TextureColorSpace>(csNode["Emission"].as<int>(0));
			material->m_NormalColorSpace = static_cast<TextureColorSpace>(csNode["Normal"].as<int>(1));
			material->m_MetallicColorSpace = static_cast<TextureColorSpace>(csNode["Metallic"].as<int>(1));
			material->m_RoughnessColorSpace = static_cast<TextureColorSpace>(csNode["Roughness"].as<int>(1));
			material->m_AOColorSpace = static_cast<TextureColorSpace>(csNode["AO"].as<int>(1));
		}

		// Alpha packing
		auto alphaNode = data["AlphaPacking"];
		if (alphaNode) {
			material->m_AlphaIsPacked = alphaNode["Enabled"].as<bool>(false);
			material->m_AlphaPackedChannel = static_cast<TextureChannel>(alphaNode["Channel"].as<int>(3));
		}

		// Detail normal maps
		auto detailNode = data["DetailNormals"];
		if (detailNode && detailNode.IsSequence()) {
			for (const auto& entry : detailNode) {
				DetailNormalMap detail;
				detail.Path = entry["Path"].as<std::string>("");
				detail.Intensity = entry["Intensity"].as<float>(1.0f);
				detail.TilingX = entry["TilingX"].as<float>(1.0f);
				detail.TilingY = entry["TilingY"].as<float>(1.0f);
				detail.OffsetX = entry["OffsetX"].as<float>(0.0f);
				detail.OffsetY = entry["OffsetY"].as<float>(0.0f);

				if (!detail.Path.empty() && std::filesystem::exists(detail.Path)) {
					detail.Texture = Texture2D::Create(detail.Path);
				}

				material->m_DetailNormalMaps.push_back(detail);
			}
		}

		// Layered texture
		auto layeredNode = data["LayeredTexture"];
		if (layeredNode) {
			material->m_LayeredConfig.Enabled = true;
			material->m_LayeredConfig.Path = layeredNode["Path"].as<std::string>("");
			material->m_LayeredConfig.MetallicChannel = static_cast<TextureChannel>(layeredNode["MetallicChannel"].as<int>(0));
			material->m_LayeredConfig.RoughnessChannel = static_cast<TextureChannel>(layeredNode["RoughnessChannel"].as<int>(1));
			material->m_LayeredConfig.AOChannel = static_cast<TextureChannel>(layeredNode["AOChannel"].as<int>(2));
			material->m_LayeredConfig.UseForMetallic = layeredNode["UseForMetallic"].as<bool>(true);
			material->m_LayeredConfig.UseForRoughness = layeredNode["UseForRoughness"].as<bool>(true);
			material->m_LayeredConfig.UseForAO = layeredNode["UseForAO"].as<bool>(true);

			if (!material->m_LayeredConfig.Path.empty() && std::filesystem::exists(material->m_LayeredConfig.Path)) {
				material->m_LayeredConfig.Texture = Texture2D::Create(material->m_LayeredConfig.Path);
			}
		}

		material->ClearDirty();
		material->SetLoaded(true);
		LNX_LOG_INFO("Material loaded: {0}", path.string());
		return material;
	}

	Ref<MaterialAsset> MaterialAsset::Clone() const {
		Ref<MaterialAsset> clone = CreateRef<MaterialAsset>(m_Name + " (Clone)");

		clone->m_Albedo = m_Albedo;
		clone->m_Metallic = m_Metallic;
		clone->m_Roughness = m_Roughness;
		clone->m_Specular = m_Specular;
		clone->m_EmissionColor = m_EmissionColor;
		clone->m_EmissionIntensity = m_EmissionIntensity;
		clone->m_NormalIntensity = m_NormalIntensity;

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

		clone->m_AlbedoColorSpace = m_AlbedoColorSpace;
		clone->m_EmissionColorSpace = m_EmissionColorSpace;
		clone->m_NormalColorSpace = m_NormalColorSpace;
		clone->m_MetallicColorSpace = m_MetallicColorSpace;
		clone->m_RoughnessColorSpace = m_RoughnessColorSpace;
		clone->m_AOColorSpace = m_AOColorSpace;

		clone->m_AlphaIsPacked = m_AlphaIsPacked;
		clone->m_AlphaPackedChannel = m_AlphaPackedChannel;

		clone->m_DetailNormalMaps = m_DetailNormalMaps;
		clone->m_LayeredConfig = m_LayeredConfig;

		clone->MarkDirty();
		return clone;
	}

	MaterialAsset::MaterialUniformData MaterialAsset::GetUniformData() const {
		MaterialUniformData data;
		memset(&data, 0, sizeof(data));

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

		// Detail normals
		data.DetailNormalCount = static_cast<int>(std::min(m_DetailNormalMaps.size(), (size_t)MAX_DETAIL_NORMALS));
		for (int i = 0; i < data.DetailNormalCount; ++i) {
			data.DetailNormalIntensities[i] = m_DetailNormalMaps[i].Intensity;
			data.DetailNormalTilingX[i] = m_DetailNormalMaps[i].TilingX;
			data.DetailNormalTilingY[i] = m_DetailNormalMaps[i].TilingY;
		}

		// Layered texture
		data.UseLayeredTexture = HasLayeredTexture() ? 1 : 0;
		if (HasLayeredTexture()) {
			data.LayeredMetallicChannel = static_cast<int>(m_LayeredConfig.MetallicChannel);
			data.LayeredRoughnessChannel = static_cast<int>(m_LayeredConfig.RoughnessChannel);
			data.LayeredAOChannel = static_cast<int>(m_LayeredConfig.AOChannel);
			data.LayeredUseMetallic = m_LayeredConfig.UseForMetallic ? 1 : 0;
			data.LayeredUseRoughness = m_LayeredConfig.UseForRoughness ? 1 : 0;
			data.LayeredUseAO = m_LayeredConfig.UseForAO ? 1 : 0;
		}

		return data;
	}

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

} // namespace Lunex
