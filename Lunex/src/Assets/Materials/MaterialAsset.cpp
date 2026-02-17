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
		}
		MarkDirty();
	}

	void MaterialAsset::SetNormalMap(Ref<Texture2D> texture) {
		m_NormalMap = texture;
		if (texture && texture->IsLoaded()) {
			m_NormalPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetMetallicMap(Ref<Texture2D> texture) {
		m_MetallicMap = texture;
		if (texture && texture->IsLoaded()) {
			m_MetallicPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetRoughnessMap(Ref<Texture2D> texture) {
		m_RoughnessMap = texture;
		if (texture && texture->IsLoaded()) {
			m_RoughnessPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetSpecularMap(Ref<Texture2D> texture) {
		m_SpecularMap = texture;
		if (texture && texture->IsLoaded()) {
			m_SpecularPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetEmissionMap(Ref<Texture2D> texture) {
		m_EmissionMap = texture;
		if (texture && texture->IsLoaded()) {
			m_EmissionPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetAOMap(Ref<Texture2D> texture) {
		m_AOMap = texture;
		if (texture && texture->IsLoaded()) {
			m_AOPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetLayeredMap(Ref<Texture2D> texture) {
		m_LayeredMap = texture;
		if (texture && texture->IsLoaded()) {
			m_LayeredPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetHeightMap(Ref<Texture2D> texture) {
		m_HeightMap = texture;
		if (texture && texture->IsLoaded()) {
			m_HeightPath = texture->GetPath();
		}
		MarkDirty();
	}

	void MaterialAsset::SetDetailNormalMap(Ref<Texture2D> texture) {
		m_DetailNormalMap = texture;
		if (texture && texture->IsLoaded()) {
			m_DetailNormalPath = texture->GetPath();
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

		// Surface Settings
		out << YAML::Key << "Surface" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "AlphaMode" << YAML::Value << AlphaModeToString(m_AlphaMode);
		out << YAML::Key << "AlphaCutoff" << YAML::Value << m_AlphaCutoff;
		out << YAML::Key << "TwoSided" << YAML::Value << m_TwoSided;
		out << YAML::Key << "UVTiling" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< m_UVTiling.x << m_UVTiling.y << YAML::EndSeq;
		out << YAML::Key << "UVOffset" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< m_UVOffset.x << m_UVOffset.y << YAML::EndSeq;
		out << YAML::Key << "FlipNormalMapY" << YAML::Value << m_FlipNormalMapY;
		out << YAML::EndMap;

		// Texture Color Spaces
		out << YAML::Key << "ColorSpaces" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Albedo" << YAML::Value << TextureColorSpaceToString(m_AlbedoColorSpace);
		out << YAML::Key << "Normal" << YAML::Value << TextureColorSpaceToString(m_NormalColorSpace);
		out << YAML::Key << "Layered" << YAML::Value << TextureColorSpaceToString(m_LayeredColorSpace);
		out << YAML::Key << "Emission" << YAML::Value << TextureColorSpaceToString(m_EmissionColorSpace);
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
		SerializeTexture(out, "Height", m_HeightPath);
		SerializeTexture(out, "DetailNormal", m_DetailNormalPath);
		out << YAML::EndMap;

		// Texture Multipliers
		out << YAML::Key << "Multipliers" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Metallic" << YAML::Value << m_MetallicMultiplier;
		out << YAML::Key << "Roughness" << YAML::Value << m_RoughnessMultiplier;
		out << YAML::Key << "Specular" << YAML::Value << m_SpecularMultiplier;
		out << YAML::Key << "AO" << YAML::Value << m_AOMultiplier;
		out << YAML::Key << "HeightScale" << YAML::Value << m_HeightScale;
		out << YAML::Key << "DetailNormalScale" << YAML::Value << m_DetailNormalScale;
		out << YAML::Key << "DetailUVTiling" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< m_DetailUVTiling.x << m_DetailUVTiling.y << YAML::EndSeq;
		out << YAML::EndMap;

		// Layered (ORM) texture
		out << YAML::Key << "LayeredTexture" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Enabled" << YAML::Value << m_UseLayeredMap;
		SerializeTexture(out, "Path", m_LayeredPath);
		out << YAML::Key << "ChannelMetallic" << YAML::Value << m_LayeredChannelMetallic;
		out << YAML::Key << "ChannelRoughness" << YAML::Value << m_LayeredChannelRoughness;
		out << YAML::Key << "ChannelAO" << YAML::Value << m_LayeredChannelAO;
		out << YAML::EndMap;

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

		// Surface Settings
		auto surfaceNode = data["Surface"];
		if (surfaceNode) {
			material->m_AlphaMode = StringToAlphaMode(surfaceNode["AlphaMode"].as<std::string>("Opaque"));
			material->m_AlphaCutoff = surfaceNode["AlphaCutoff"].as<float>(0.5f);
			material->m_TwoSided = surfaceNode["TwoSided"].as<bool>(false);

			auto uvTiling = surfaceNode["UVTiling"];
			if (uvTiling) {
				material->m_UVTiling = glm::vec2(uvTiling[0].as<float>(1.0f), uvTiling[1].as<float>(1.0f));
			}

			auto uvOffset = surfaceNode["UVOffset"];
			if (uvOffset) {
				material->m_UVOffset = glm::vec2(uvOffset[0].as<float>(0.0f), uvOffset[1].as<float>(0.0f));
			}

			material->m_FlipNormalMapY = surfaceNode["FlipNormalMapY"].as<bool>(false);
		}

		// Texture Color Spaces
		auto colorSpacesNode = data["ColorSpaces"];
		if (colorSpacesNode) {
			material->m_AlbedoColorSpace = StringToTextureColorSpace(colorSpacesNode["Albedo"].as<std::string>("sRGB"));
			material->m_NormalColorSpace = StringToTextureColorSpace(colorSpacesNode["Normal"].as<std::string>("Linear"));
			material->m_LayeredColorSpace = StringToTextureColorSpace(colorSpacesNode["Layered"].as<std::string>("Linear"));
			material->m_EmissionColorSpace = StringToTextureColorSpace(colorSpacesNode["Emission"].as<std::string>("sRGB"));
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
			material->DeserializeTexture(texturesNode, "Height", material->m_HeightMap, material->m_HeightPath);
			material->DeserializeTexture(texturesNode, "DetailNormal", material->m_DetailNormalMap, material->m_DetailNormalPath);
		}

		// Multipliers
		auto multipliersNode = data["Multipliers"];
		if (multipliersNode) {
			material->m_MetallicMultiplier = multipliersNode["Metallic"].as<float>(1.0f);
			material->m_RoughnessMultiplier = multipliersNode["Roughness"].as<float>(1.0f);
			material->m_SpecularMultiplier = multipliersNode["Specular"].as<float>(1.0f);
			material->m_AOMultiplier = multipliersNode["AO"].as<float>(1.0f);
			material->m_HeightScale = multipliersNode["HeightScale"].as<float>(0.05f);
			material->m_DetailNormalScale = multipliersNode["DetailNormalScale"].as<float>(1.0f);

			auto detailUV = multipliersNode["DetailUVTiling"];
			if (detailUV) {
				material->m_DetailUVTiling = glm::vec2(detailUV[0].as<float>(4.0f), detailUV[1].as<float>(4.0f));
			}
		}

		// Layered (ORM) texture
		auto layeredNode = data["LayeredTexture"];
		if (layeredNode) {
			material->m_UseLayeredMap = layeredNode["Enabled"].as<bool>(false);
			material->DeserializeTexture(layeredNode, "Path", material->m_LayeredMap, material->m_LayeredPath);
			material->m_LayeredChannelMetallic = layeredNode["ChannelMetallic"].as<int>(0);
			material->m_LayeredChannelRoughness = layeredNode["ChannelRoughness"].as<int>(1);
			material->m_LayeredChannelAO = layeredNode["ChannelAO"].as<int>(2);
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

		// Normal map settings
		clone->m_FlipNormalMapY = m_FlipNormalMapY;

		// Surface settings
		clone->m_AlphaMode = m_AlphaMode;
		clone->m_AlphaCutoff = m_AlphaCutoff;
		clone->m_TwoSided = m_TwoSided;
		clone->m_UVTiling = m_UVTiling;
		clone->m_UVOffset = m_UVOffset;

		// Color spaces
		clone->m_AlbedoColorSpace = m_AlbedoColorSpace;
		clone->m_NormalColorSpace = m_NormalColorSpace;
		clone->m_LayeredColorSpace = m_LayeredColorSpace;
		clone->m_EmissionColorSpace = m_EmissionColorSpace;

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

		// Layered
		clone->m_LayeredMap = m_LayeredMap;
		clone->m_LayeredPath = m_LayeredPath;
		clone->m_UseLayeredMap = m_UseLayeredMap;
		clone->m_LayeredChannelMetallic = m_LayeredChannelMetallic;
		clone->m_LayeredChannelRoughness = m_LayeredChannelRoughness;
		clone->m_LayeredChannelAO = m_LayeredChannelAO;

		// Height
		clone->m_HeightMap = m_HeightMap;
		clone->m_HeightPath = m_HeightPath;
		clone->m_HeightScale = m_HeightScale;

		// Detail Normal
		clone->m_DetailNormalMap = m_DetailNormalMap;
		clone->m_DetailNormalPath = m_DetailNormalPath;
		clone->m_DetailNormalScale = m_DetailNormalScale;
		clone->m_DetailUVTiling = m_DetailUVTiling;

		clone->MarkDirty();
		return clone;
	}

	MaterialAsset::MaterialUniformData MaterialAsset::GetUniformData() const {
		MaterialUniformData data{};

		data.Albedo = m_Albedo;
		data.Metallic = m_Metallic;
		data.Roughness = m_Roughness;
		data.Specular = m_Specular;
		data.EmissionIntensity = m_EmissionIntensity;
		data.EmissionColor = m_EmissionColor;
		data.NormalIntensity = m_NormalIntensity;

		bool layeredActive = m_UseLayeredMap && HasLayeredMap();

		data.UseAlbedoMap = HasAlbedoMap() ? 1 : 0;
		data.UseNormalMap = HasNormalMap() ? 1 : 0;
		data.UseMetallicMap = (HasMetallicMap() || layeredActive) ? 1 : 0;
		data.UseRoughnessMap = (HasRoughnessMap() || layeredActive) ? 1 : 0;
		data.UseSpecularMap = HasSpecularMap() ? 1 : 0;
		data.UseEmissionMap = HasEmissionMap() ? 1 : 0;
		data.UseAOMap = (HasAOMap() || layeredActive) ? 1 : 0;
		data.UseLayeredMap = layeredActive ? 1 : 0;

		data.MetallicMultiplier = m_MetallicMultiplier;
		data.RoughnessMultiplier = m_RoughnessMultiplier;
		data.SpecularMultiplier = m_SpecularMultiplier;
		data.AOMultiplier = m_AOMultiplier;

		data.UVTiling = m_UVTiling;
		data.UVOffset = m_UVOffset;

		data.LayeredChannelMetallic = m_LayeredChannelMetallic;
		data.LayeredChannelRoughness = m_LayeredChannelRoughness;
		data.LayeredChannelAO = m_LayeredChannelAO;
		data.UseHeightMap = HasHeightMap() ? 1 : 0;

		data.HeightScale = m_HeightScale;
		data.UseDetailNormalMap = HasDetailNormalMap() ? 1 : 0;
		data.DetailNormalScale = m_DetailNormalScale;
		data.AlphaCutoff = m_AlphaCutoff;

		data.DetailUVTiling = m_DetailUVTiling;
		data.AlphaMode = static_cast<int>(m_AlphaMode);
		data.FlipNormalMapY = m_FlipNormalMapY ? 1 : 0;

		data.AlbedoColorSpace = static_cast<int>(m_AlbedoColorSpace);
		data.NormalColorSpace = static_cast<int>(m_NormalColorSpace);
		data.LayeredColorSpace = static_cast<int>(m_LayeredColorSpace);
		data.EmissionColorSpace = static_cast<int>(m_EmissionColorSpace);

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
