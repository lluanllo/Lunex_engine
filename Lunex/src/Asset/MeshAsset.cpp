#include "stpch.h"
#include "MeshAsset.h"
#include "Log/Log.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Lunex {

	MeshAsset::MeshAsset()
		: Asset()
	{
		m_Name = "New Mesh";
	}

	MeshAsset::MeshAsset(const std::string& name)
		: Asset(name)
	{
	}

	// ========== SOURCE FILE ==========

	bool MeshAsset::HasValidSource() const {
		return !m_SourcePath.empty() && std::filesystem::exists(m_SourcePath);
	}

	bool MeshAsset::NeedsReimport() const {
		if (!HasValidSource()) return false;
		
		try {
			auto currentModified = std::filesystem::last_write_time(m_SourcePath);
			return currentModified != m_SourceLastModified;
		}
		catch (const std::exception&) {
			return false;
		}
	}

	// ========== RUNTIME MODEL ==========

	Ref<Model> MeshAsset::GetModel() {
		if (m_Model) {
			return m_Model;
		}
		
		// Load from source file
		if (!HasValidSource()) {
			LNX_LOG_ERROR("MeshAsset::GetModel - No valid source file for: {0}", m_Name);
			return nullptr;
		}
		
		m_Model = CreateRef<Model>(m_SourcePath.string());
		
		if (m_Model && !m_Model->GetMeshes().empty()) {
			SetLoaded(true);
			CalculateMetadata();
			LNX_LOG_INFO("Mesh loaded: {0} ({1} vertices, {2} triangles)", 
				m_Name, m_Metadata.VertexCount, m_Metadata.TriangleCount);
		}
		else {
			LNX_LOG_ERROR("MeshAsset::GetModel - Failed to load model from: {0}", m_SourcePath.string());
			m_Model = nullptr;
		}
		
		return m_Model;
	}

	void MeshAsset::ReloadModel() {
		UnloadModel();
		GetModel();
	}

	void MeshAsset::UnloadModel() {
		m_Model.reset();
		SetLoaded(false);
	}

	// ========== SERIALIZATION ==========

	bool MeshAsset::SaveToFile(const std::filesystem::path& path) {
		m_FilePath = path;
		
		if (m_FilePath.empty()) {
			LNX_LOG_ERROR("MeshAsset::SaveToFile - No file path specified");
			return false;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;

		// Header
		out << YAML::Key << "MeshAsset" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "ID" << YAML::Value << (uint64_t)m_ID;
		out << YAML::Key << "Name" << YAML::Value << m_Name;
		out << YAML::Key << "SourcePath" << YAML::Value << m_SourcePath.string();
		out << YAML::EndMap;

		// Import Settings
		SerializeImportSettings(out);

		// Metadata
		SerializeMetadata(out);

		out << YAML::EndMap;

		// Write to file
		std::ofstream fout(m_FilePath);
		if (!fout.is_open()) {
			LNX_LOG_ERROR("MeshAsset::SaveToFile - Failed to open file: {0}", m_FilePath.string());
			return false;
		}

		fout << out.c_str();
		fout.close();

		ClearDirty();
		LNX_LOG_INFO("MeshAsset saved: {0}", m_FilePath.string());
		return true;
	}

	Ref<MeshAsset> MeshAsset::LoadFromFile(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("MeshAsset::LoadFromFile - File not found: {0}", path.string());
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
			LNX_LOG_ERROR("MeshAsset::LoadFromFile - Failed to parse YAML: {0}", e.what());
			return nullptr;
		}

		auto meshNode = data["MeshAsset"];
		if (!meshNode) {
			LNX_LOG_ERROR("MeshAsset::LoadFromFile - Invalid mesh asset file format");
			return nullptr;
		}

		Ref<MeshAsset> meshAsset = CreateRef<MeshAsset>();
		meshAsset->m_FilePath = path;

		// Header
		meshAsset->m_ID = UUID(meshNode["ID"].as<uint64_t>());
		meshAsset->m_Name = meshNode["Name"].as<std::string>();
		
		if (meshNode["SourcePath"]) {
			meshAsset->m_SourcePath = meshNode["SourcePath"].as<std::string>();
			
			// Update source file timestamp
			if (meshAsset->HasValidSource()) {
				meshAsset->m_SourceLastModified = std::filesystem::last_write_time(meshAsset->m_SourcePath);
			}
		}

		// Import Settings
		meshAsset->DeserializeImportSettings(data["ImportSettings"]);

		// Metadata
		meshAsset->DeserializeMetadata(data["Metadata"]);

		meshAsset->ClearDirty();
		LNX_LOG_INFO("MeshAsset loaded: {0} (source: {1})", path.string(), meshAsset->m_SourcePath.string());
		return meshAsset;
	}

	// ========== IMPORT ==========

	Ref<MeshAsset> MeshAsset::Import(const std::filesystem::path& sourcePath, const MeshImportSettings& settings) {
		if (!std::filesystem::exists(sourcePath)) {
			LNX_LOG_ERROR("MeshAsset::Import - Source file not found: {0}", sourcePath.string());
			return nullptr;
		}

		// Create new mesh asset
		std::string name = sourcePath.stem().string();
		Ref<MeshAsset> meshAsset = CreateRef<MeshAsset>(name);
		
		meshAsset->m_SourcePath = sourcePath;
		meshAsset->m_ImportSettings = settings;
		
		// Store source file timestamp
		meshAsset->m_SourceLastModified = std::filesystem::last_write_time(sourcePath);

		// Load model to calculate metadata
		meshAsset->m_Model = CreateRef<Model>(sourcePath.string());
		
		if (!meshAsset->m_Model || meshAsset->m_Model->GetMeshes().empty()) {
			LNX_LOG_ERROR("MeshAsset::Import - Failed to load model from: {0}", sourcePath.string());
			return nullptr;
		}

		meshAsset->CalculateMetadata();
		meshAsset->SetLoaded(true);
		meshAsset->MarkDirty();  // Needs to be saved

		LNX_LOG_INFO("MeshAsset imported: {0} ({1} vertices, {2} triangles, {3} submeshes)",
			name, meshAsset->m_Metadata.VertexCount, meshAsset->m_Metadata.TriangleCount, 
			meshAsset->m_Metadata.SubmeshCount);

		return meshAsset;
	}

	bool MeshAsset::Reimport() {
		if (!HasValidSource()) {
			LNX_LOG_ERROR("MeshAsset::Reimport - No valid source file");
			return false;
		}

		// Reload model
		m_Model = CreateRef<Model>(m_SourcePath.string());
		
		if (!m_Model || m_Model->GetMeshes().empty()) {
			LNX_LOG_ERROR("MeshAsset::Reimport - Failed to reload model from: {0}", m_SourcePath.string());
			m_Model = nullptr;
			return false;
		}

		// Update timestamp and metadata
		m_SourceLastModified = std::filesystem::last_write_time(m_SourcePath);
		CalculateMetadata();
		SetLoaded(true);
		MarkDirty();

		LNX_LOG_INFO("MeshAsset reimported: {0}", m_Name);
		return true;
	}

	// ========== PRIVATE HELPERS ==========

	void MeshAsset::CalculateMetadata() {
		if (!m_Model) return;

		m_Metadata = MeshMetadata();

		glm::vec3 boundsMin = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

		for (const auto& mesh : m_Model->GetMeshes()) {
			const auto& vertices = mesh->GetVertices();
			const auto& indices = mesh->GetIndices();

			m_Metadata.VertexCount += static_cast<uint32_t>(vertices.size());
			m_Metadata.IndexCount += static_cast<uint32_t>(indices.size());
			m_Metadata.SubmeshCount++;

			// Calculate bounds
			for (const auto& vertex : vertices) {
				boundsMin = glm::min(boundsMin, vertex.Position);
				boundsMax = glm::max(boundsMax, vertex.Position);
			}

			// Calculate buffer sizes
			m_Metadata.VertexBufferSize += vertices.size() * sizeof(Vertex);
			m_Metadata.IndexBufferSize += indices.size() * sizeof(uint32_t);
		}

		m_Metadata.TriangleCount = m_Metadata.IndexCount / 3;
		m_Metadata.BoundsMin = boundsMin;
		m_Metadata.BoundsMax = boundsMax;
		m_Metadata.BoundsCenter = (boundsMin + boundsMax) * 0.5f;
		m_Metadata.BoundsRadius = glm::length(boundsMax - m_Metadata.BoundsCenter);
	}

	void MeshAsset::SerializeImportSettings(YAML::Emitter& out) const {
		out << YAML::Key << "ImportSettings" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Scale" << YAML::Value << m_ImportSettings.Scale;
		out << YAML::Key << "Rotation" << YAML::Value << YAML::Flow << YAML::BeginSeq 
			<< m_ImportSettings.Rotation.x << m_ImportSettings.Rotation.y << m_ImportSettings.Rotation.z << YAML::EndSeq;
		out << YAML::Key << "Translation" << YAML::Value << YAML::Flow << YAML::BeginSeq 
			<< m_ImportSettings.Translation.x << m_ImportSettings.Translation.y << m_ImportSettings.Translation.z << YAML::EndSeq;
		out << YAML::Key << "FlipUVs" << YAML::Value << m_ImportSettings.FlipUVs;
		out << YAML::Key << "GenerateNormals" << YAML::Value << m_ImportSettings.GenerateNormals;
		out << YAML::Key << "GenerateTangents" << YAML::Value << m_ImportSettings.GenerateTangents;
		out << YAML::Key << "OptimizeMesh" << YAML::Value << m_ImportSettings.OptimizeMesh;
		out << YAML::Key << "GenerateLODs" << YAML::Value << m_ImportSettings.GenerateLODs;
		out << YAML::Key << "LODLevels" << YAML::Value << m_ImportSettings.LODLevels;
		out << YAML::Key << "LODReductionFactor" << YAML::Value << m_ImportSettings.LODReductionFactor;
		out << YAML::Key << "GenerateCollision" << YAML::Value << m_ImportSettings.GenerateCollision;
		out << YAML::Key << "UseConvexCollision" << YAML::Value << m_ImportSettings.UseConvexCollision;
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
			<< m_Metadata.BoundsMin.x << m_Metadata.BoundsMin.y << m_Metadata.BoundsMin.z << YAML::EndSeq;
		out << YAML::Key << "BoundsMax" << YAML::Value << YAML::Flow << YAML::BeginSeq 
			<< m_Metadata.BoundsMax.x << m_Metadata.BoundsMax.y << m_Metadata.BoundsMax.z << YAML::EndSeq;
		out << YAML::Key << "BoundsCenter" << YAML::Value << YAML::Flow << YAML::BeginSeq 
			<< m_Metadata.BoundsCenter.x << m_Metadata.BoundsCenter.y << m_Metadata.BoundsCenter.z << YAML::EndSeq;
		out << YAML::Key << "BoundsRadius" << YAML::Value << m_Metadata.BoundsRadius;
		out << YAML::Key << "VertexBufferSize" << YAML::Value << m_Metadata.VertexBufferSize;
		out << YAML::Key << "IndexBufferSize" << YAML::Value << m_Metadata.IndexBufferSize;
		
		// Material names
		if (!m_Metadata.MaterialNames.empty()) {
			out << YAML::Key << "MaterialNames" << YAML::Value << YAML::BeginSeq;
			for (const auto& name : m_Metadata.MaterialNames) {
				out << name;
			}
			out << YAML::EndSeq;
		}
		
		out << YAML::EndMap;
	}

	void MeshAsset::DeserializeImportSettings(const YAML::Node& node) {
		if (!node) return;

		m_ImportSettings.Scale = node["Scale"].as<float>(1.0f);
		
		if (node["Rotation"]) {
			m_ImportSettings.Rotation = glm::vec3(
				node["Rotation"][0].as<float>(0.0f),
				node["Rotation"][1].as<float>(0.0f),
				node["Rotation"][2].as<float>(0.0f)
			);
		}
		
		if (node["Translation"]) {
			m_ImportSettings.Translation = glm::vec3(
				node["Translation"][0].as<float>(0.0f),
				node["Translation"][1].as<float>(0.0f),
				node["Translation"][2].as<float>(0.0f)
			);
		}

		m_ImportSettings.FlipUVs = node["FlipUVs"].as<bool>(false);
		m_ImportSettings.GenerateNormals = node["GenerateNormals"].as<bool>(true);
		m_ImportSettings.GenerateTangents = node["GenerateTangents"].as<bool>(true);
		m_ImportSettings.OptimizeMesh = node["OptimizeMesh"].as<bool>(true);
		m_ImportSettings.GenerateLODs = node["GenerateLODs"].as<bool>(false);
		m_ImportSettings.LODLevels = node["LODLevels"].as<int>(3);
		m_ImportSettings.LODReductionFactor = node["LODReductionFactor"].as<float>(0.5f);
		m_ImportSettings.GenerateCollision = node["GenerateCollision"].as<bool>(false);
		m_ImportSettings.UseConvexCollision = node["UseConvexCollision"].as<bool>(true);
	}

	void MeshAsset::DeserializeMetadata(const YAML::Node& node) {
		if (!node) return;

		m_Metadata.VertexCount = node["VertexCount"].as<uint32_t>(0);
		m_Metadata.IndexCount = node["IndexCount"].as<uint32_t>(0);
		m_Metadata.TriangleCount = node["TriangleCount"].as<uint32_t>(0);
		m_Metadata.SubmeshCount = node["SubmeshCount"].as<uint32_t>(0);

		if (node["BoundsMin"]) {
			m_Metadata.BoundsMin = glm::vec3(
				node["BoundsMin"][0].as<float>(0.0f),
				node["BoundsMin"][1].as<float>(0.0f),
				node["BoundsMin"][2].as<float>(0.0f)
			);
		}

		if (node["BoundsMax"]) {
			m_Metadata.BoundsMax = glm::vec3(
				node["BoundsMax"][0].as<float>(0.0f),
				node["BoundsMax"][1].as<float>(0.0f),
				node["BoundsMax"][2].as<float>(0.0f)
			);
		}

		if (node["BoundsCenter"]) {
			m_Metadata.BoundsCenter = glm::vec3(
				node["BoundsCenter"][0].as<float>(0.0f),
				node["BoundsCenter"][1].as<float>(0.0f),
				node["BoundsCenter"][2].as<float>(0.0f)
			);
		}

		m_Metadata.BoundsRadius = node["BoundsRadius"].as<float>(0.0f);
		m_Metadata.VertexBufferSize = node["VertexBufferSize"].as<size_t>(0);
		m_Metadata.IndexBufferSize = node["IndexBufferSize"].as<size_t>(0);

		if (node["MaterialNames"]) {
			for (const auto& name : node["MaterialNames"]) {
				m_Metadata.MaterialNames.push_back(name.as<std::string>());
			}
		}
	}

}
