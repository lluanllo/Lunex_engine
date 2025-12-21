#include "stpch.h"
#include "MeshAsset.h"
#include "Log/Log.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Lunex {

	MeshAsset::MeshAsset()
		: Asset() {
	}

	MeshAsset::MeshAsset(const std::string& name)
		: Asset(name) {
	}

	bool MeshAsset::HasValidSource() const {
		return !m_SourcePath.empty() && std::filesystem::exists(m_SourcePath);
	}

	bool MeshAsset::NeedsReimport() const {
		if (!HasValidSource()) return false;
		
		auto currentModified = std::filesystem::last_write_time(m_SourcePath);
		return currentModified != m_SourceLastModified;
	}

	Ref<Model> MeshAsset::GetModel() {
		if (!m_Model && HasValidSource()) {
			ReloadModel();
		}
		return m_Model;
	}

	void MeshAsset::ReloadModel() {
		if (!HasValidSource()) {
			LNX_LOG_ERROR("MeshAsset::ReloadModel - No valid source file");
			return;
		}
		
		m_Model = CreateRef<Model>(m_SourcePath.string());
		
		if (m_Model) {
			CalculateMetadata();
			SetLoaded(true);
		}
	}

	void MeshAsset::UnloadModel() {
		m_Model = nullptr;
		SetLoaded(false);
	}

	void MeshAsset::CalculateMetadata() {
		if (!m_Model) return;
		
		m_Metadata.VertexCount = 0;
		m_Metadata.IndexCount = 0;
		m_Metadata.SubmeshCount = static_cast<uint32_t>(m_Model->GetMeshes().size());
		
		m_Metadata.BoundsMin = glm::vec3(std::numeric_limits<float>::max());
		m_Metadata.BoundsMax = glm::vec3(std::numeric_limits<float>::lowest());
		
		for (const auto& mesh : m_Model->GetMeshes()) {
			auto& vertices = mesh->GetVertices();
			auto& indices = mesh->GetIndices();
			
			m_Metadata.VertexCount += static_cast<uint32_t>(vertices.size());
			m_Metadata.IndexCount += static_cast<uint32_t>(indices.size());
			
			for (const auto& vertex : vertices) {
				m_Metadata.BoundsMin = glm::min(m_Metadata.BoundsMin, vertex.Position);
				m_Metadata.BoundsMax = glm::max(m_Metadata.BoundsMax, vertex.Position);
			}
		}
		
		m_Metadata.TriangleCount = m_Metadata.IndexCount / 3;
		m_Metadata.BoundsCenter = (m_Metadata.BoundsMin + m_Metadata.BoundsMax) * 0.5f;
		m_Metadata.BoundsRadius = glm::length(m_Metadata.BoundsMax - m_Metadata.BoundsCenter);
	}

	bool MeshAsset::SaveToFile(const std::filesystem::path& path) {
		YAML::Emitter out;
		out << YAML::BeginMap;
		
		out << YAML::Key << "MeshAsset" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "ID" << YAML::Value << (uint64_t)m_ID;
		out << YAML::Key << "Name" << YAML::Value << m_Name;
		out << YAML::Key << "SourcePath" << YAML::Value << m_SourcePath.string();
		out << YAML::EndMap;
		
		SerializeImportSettings(out);
		SerializeMetadata(out);
		
		out << YAML::EndMap;
		
		std::ofstream fout(path);
		if (!fout.is_open()) {
			LNX_LOG_ERROR("Failed to save MeshAsset: {0}", path.string());
			return false;
		}
		
		fout << out.c_str();
		fout.close();
		
		m_FilePath = path;
		ClearDirty();
		
		return true;
	}

	Ref<MeshAsset> MeshAsset::LoadFromFile(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("MeshAsset file not found: {0}", path.string());
			return nullptr;
		}
		
		try {
			YAML::Node data = YAML::LoadFile(path.string());
			
			auto meshNode = data["MeshAsset"];
			if (!meshNode) {
				LNX_LOG_ERROR("Invalid MeshAsset file: {0}", path.string());
				return nullptr;
			}
			
			auto asset = CreateRef<MeshAsset>();
			asset->m_ID = UUID(meshNode["ID"].as<uint64_t>());
			asset->m_Name = meshNode["Name"].as<std::string>();
			asset->m_SourcePath = meshNode["SourcePath"].as<std::string>();
			asset->m_FilePath = path;
			
			asset->DeserializeImportSettings(data["ImportSettings"]);
			asset->DeserializeMetadata(data["Metadata"]);
			
			if (asset->HasValidSource()) {
				asset->m_SourceLastModified = std::filesystem::last_write_time(asset->m_SourcePath);
			}
			
			return asset;
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to load MeshAsset: {0}", e.what());
			return nullptr;
		}
	}

	Ref<MeshAsset> MeshAsset::Import(const std::filesystem::path& sourcePath, const MeshImportSettings& settings) {
		if (!std::filesystem::exists(sourcePath)) {
			LNX_LOG_ERROR("Source file not found: {0}", sourcePath.string());
			return nullptr;
		}
		
		auto asset = CreateRef<MeshAsset>(sourcePath.stem().string());
		asset->SetSourcePath(sourcePath);
		asset->m_ImportSettings = settings;
		asset->m_SourceLastModified = std::filesystem::last_write_time(sourcePath);
		
		// Load the model
		asset->ReloadModel();
		
		if (!asset->m_Model) {
			LNX_LOG_ERROR("Failed to load model: {0}", sourcePath.string());
			return nullptr;
		}
		
		return asset;
	}

	bool MeshAsset::Reimport() {
		if (!HasValidSource()) {
			LNX_LOG_ERROR("Cannot reimport - no valid source file");
			return false;
		}
		
		ReloadModel();
		m_SourceLastModified = std::filesystem::last_write_time(m_SourcePath);
		MarkDirty();
		
		return m_Model != nullptr;
	}

	void MeshAsset::SerializeImportSettings(YAML::Emitter& out) const {
		out << YAML::Key << "ImportSettings" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Scale" << YAML::Value << m_ImportSettings.Scale;
		out << YAML::Key << "Rotation" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< m_ImportSettings.Rotation.x << m_ImportSettings.Rotation.y << m_ImportSettings.Rotation.z
			<< YAML::EndSeq;
		out << YAML::Key << "Translation" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< m_ImportSettings.Translation.x << m_ImportSettings.Translation.y << m_ImportSettings.Translation.z
			<< YAML::EndSeq;
		out << YAML::Key << "FlipUVs" << YAML::Value << m_ImportSettings.FlipUVs;
		out << YAML::Key << "GenerateNormals" << YAML::Value << m_ImportSettings.GenerateNormals;
		out << YAML::Key << "GenerateTangents" << YAML::Value << m_ImportSettings.GenerateTangents;
		out << YAML::Key << "OptimizeMesh" << YAML::Value << m_ImportSettings.OptimizeMesh;
		out << YAML::EndMap;
	}

	void MeshAsset::SerializeMetadata(YAML::Emitter& out) const {
		out << YAML::Key << "Metadata" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "VertexCount" << YAML::Value << m_Metadata.VertexCount;
		out << YAML::Key << "IndexCount" << YAML::Value << m_Metadata.IndexCount;
		out << YAML::Key << "TriangleCount" << YAML::Value << m_Metadata.TriangleCount;
		out << YAML::Key << "SubmeshCount" << YAML::Value << m_Metadata.SubmeshCount;
		out << YAML::Key << "BoundsMin" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< m_Metadata.BoundsMin.x << m_Metadata.BoundsMin.y << m_Metadata.BoundsMin.z
			<< YAML::EndSeq;
		out << YAML::Key << "BoundsMax" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< m_Metadata.BoundsMax.x << m_Metadata.BoundsMax.y << m_Metadata.BoundsMax.z
			<< YAML::EndSeq;
		out << YAML::EndMap;
	}

	void MeshAsset::DeserializeImportSettings(const YAML::Node& node) {
		if (!node) return;
		
		m_ImportSettings.Scale = node["Scale"].as<float>(1.0f);
		
		if (node["Rotation"]) {
			auto rot = node["Rotation"];
			m_ImportSettings.Rotation = { rot[0].as<float>(), rot[1].as<float>(), rot[2].as<float>() };
		}
		
		if (node["Translation"]) {
			auto trans = node["Translation"];
			m_ImportSettings.Translation = { trans[0].as<float>(), trans[1].as<float>(), trans[2].as<float>() };
		}
		
		m_ImportSettings.FlipUVs = node["FlipUVs"].as<bool>(false);
		m_ImportSettings.GenerateNormals = node["GenerateNormals"].as<bool>(true);
		m_ImportSettings.GenerateTangents = node["GenerateTangents"].as<bool>(true);
		m_ImportSettings.OptimizeMesh = node["OptimizeMesh"].as<bool>(true);
	}

	void MeshAsset::DeserializeMetadata(const YAML::Node& node) {
		if (!node) return;
		
		m_Metadata.VertexCount = node["VertexCount"].as<uint32_t>(0);
		m_Metadata.IndexCount = node["IndexCount"].as<uint32_t>(0);
		m_Metadata.TriangleCount = node["TriangleCount"].as<uint32_t>(0);
		m_Metadata.SubmeshCount = node["SubmeshCount"].as<uint32_t>(0);
		
		if (node["BoundsMin"]) {
			auto min = node["BoundsMin"];
			m_Metadata.BoundsMin = { min[0].as<float>(), min[1].as<float>(), min[2].as<float>() };
		}
		
		if (node["BoundsMax"]) {
			auto max = node["BoundsMax"];
			m_Metadata.BoundsMax = { max[0].as<float>(), max[1].as<float>(), max[2].as<float>() };
		}
		
		m_Metadata.BoundsCenter = (m_Metadata.BoundsMin + m_Metadata.BoundsMax) * 0.5f;
		m_Metadata.BoundsRadius = glm::length(m_Metadata.BoundsMax - m_Metadata.BoundsCenter);
	}

} // namespace Lunex
