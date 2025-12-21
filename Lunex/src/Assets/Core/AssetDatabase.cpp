#include "stpch.h"
#include "AssetDatabase.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Asset/Prefab.h"
#include "Log/Log.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Lunex {

	// ========== INITIALIZATION ==========

	void AssetDatabase::Initialize(const std::filesystem::path& projectRoot, const std::filesystem::path& assetsFolder) {
		m_ProjectRoot = projectRoot;
		m_AssetsFolder = assetsFolder;
		m_DatabasePath = m_ProjectRoot / ".lnxast";
		
		if (std::filesystem::exists(m_DatabasePath)) {
			if (LoadDatabase()) {
				LNX_LOG_INFO("AssetDatabase loaded from {0} ({1} assets)", 
					m_DatabasePath.string(), m_Assets.size());
			} else {
				LNX_LOG_WARN("Failed to load AssetDatabase, scanning assets...");
				ScanAssets();
				SaveDatabase();
			}
		} else {
			LNX_LOG_INFO("AssetDatabase not found, scanning assets...");
			ScanAssets();
			SaveDatabase();
		}
		
		m_IsInitialized = true;
	}

	void AssetDatabase::ScanAssets() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		m_Assets.clear();
		m_PathToID.clear();
		
		if (!std::filesystem::exists(m_AssetsFolder)) {
			LNX_LOG_ERROR("Assets folder not found: {0}", m_AssetsFolder.string());
			return;
		}
		
		ScanDirectory(m_AssetsFolder);
		
		LNX_LOG_INFO("AssetDatabase scan complete - found {0} assets", m_Assets.size());
	}

	void AssetDatabase::ScanDirectory(const std::filesystem::path& directory) {
		try {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
				if (entry.is_regular_file()) {
					std::string extension = entry.path().extension().string();
					std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
					
					AssetType type = GetAssetTypeFromExtension(extension);
					if (type != AssetType::None) {
						AssetDatabaseEntry assetEntry = ExtractAssetMetadata(entry.path());
						RegisterAsset(assetEntry);
					}
				}
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Error scanning directory {0}: {1}", directory.string(), e.what());
		}
	}

	AssetDatabaseEntry AssetDatabase::ExtractAssetMetadata(const std::filesystem::path& filePath) {
		AssetDatabaseEntry entry;
		
		entry.RelativePath = GetRelativePath(filePath);
		entry.Name = filePath.stem().string();
		entry.FileSize = std::filesystem::file_size(filePath);
		entry.LastModified = std::filesystem::last_write_time(filePath);
		
		std::string extension = filePath.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		entry.Type = GetAssetTypeFromExtension(extension);
		
		entry.AssetID = GenerateAssetID(filePath);
		entry.Dependencies = ExtractDependencies(filePath, entry.Type);
		
		return entry;
	}

	UUID AssetDatabase::GenerateAssetID(const std::filesystem::path& filePath) {
		std::string extension = filePath.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		
		try {
			if (extension == ".lumat" || extension == ".lumesh" || extension == ".luprefab") {
				YAML::Node data = YAML::LoadFile(filePath.string());
				
				if (extension == ".lumat" && data["Material"] && data["Material"]["ID"]) {
					return UUID(data["Material"]["ID"].as<uint64_t>());
				}
				else if (extension == ".lumesh" && data["MeshAsset"] && data["MeshAsset"]["ID"]) {
					return UUID(data["MeshAsset"]["ID"].as<uint64_t>());
				}
				else if (extension == ".luprefab" && data["Prefab"] && data["Prefab"]["UUID"]) {
					return UUID(data["Prefab"]["UUID"].as<uint64_t>());
				}
			}
		}
		catch (...) {
			// Generate new UUID if reading fails
		}
		
		return UUID();
	}

	std::vector<UUID> AssetDatabase::ExtractDependencies(const std::filesystem::path& filePath, AssetType type) {
		std::vector<UUID> dependencies;
		
		try {
			if (type == AssetType::Prefab) {
				YAML::Node data = YAML::LoadFile(filePath.string());
				auto entities = data["Entities"];
				if (entities && entities.IsSequence()) {
					for (const auto& entity : entities) {
						auto components = entity["Components"];
						if (components && components.IsSequence()) {
							for (const auto& comp : components) {
								std::string compType = comp["Type"].as<std::string>("");
								
								if (compType == "MeshComponent") {
									std::string compData = comp["Data"].as<std::string>("");
									std::istringstream iss(compData);
									std::string token;
									std::getline(iss, token, ';'); // type
									std::getline(iss, token, ';'); // color
									std::getline(iss, token, ';'); // meshAssetID
									if (!token.empty() && token != "0") {
										dependencies.push_back(UUID(std::stoull(token)));
									}
								}
								else if (compType == "MaterialComponent") {
									std::string compData = comp["Data"].as<std::string>("");
									std::istringstream iss(compData);
									std::string token;
									std::getline(iss, token, ';'); // assetID
									if (!token.empty() && token != "0") {
										dependencies.push_back(UUID(std::stoull(token)));
									}
								}
							}
						}
					}
				}
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_WARN("Failed to extract dependencies from {0}: {1}", 
				filePath.filename().string(), e.what());
		}
		
		return dependencies;
	}

	// ========== ASSET REGISTRATION ==========

	void AssetDatabase::RegisterAsset(const AssetDatabaseEntry& entry) {
		m_Assets[entry.AssetID] = entry;
		
		std::string pathStr = entry.RelativePath.string();
		std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
		std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
		m_PathToID[pathStr] = entry.AssetID;
	}

	void AssetDatabase::UnregisterAsset(UUID assetID) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		auto it = m_Assets.find(assetID);
		if (it != m_Assets.end()) {
			std::string pathStr = it->second.RelativePath.string();
			std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
			std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
			m_PathToID.erase(pathStr);
			
			m_Assets.erase(it);
		}
	}

	void AssetDatabase::UpdateAsset(UUID assetID, const AssetDatabaseEntry& entry) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		auto it = m_Assets.find(assetID);
		if (it != m_Assets.end()) {
			std::string oldPath = it->second.RelativePath.string();
			std::replace(oldPath.begin(), oldPath.end(), '\\', '/');
			std::transform(oldPath.begin(), oldPath.end(), oldPath.begin(), ::tolower);
			m_PathToID.erase(oldPath);
			
			it->second = entry;
			
			std::string newPath = entry.RelativePath.string();
			std::replace(newPath.begin(), newPath.end(), '\\', '/');
			std::transform(newPath.begin(), newPath.end(), newPath.begin(), ::tolower);
			m_PathToID[newPath] = assetID;
		}
	}

	// ========== QUERIES ==========

	const AssetDatabaseEntry* AssetDatabase::GetAssetEntry(UUID assetID) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		auto it = m_Assets.find(assetID);
		if (it != m_Assets.end()) {
			return &it->second;
		}
		return nullptr;
	}

	const AssetDatabaseEntry* AssetDatabase::GetAssetEntryByPath(const std::filesystem::path& path) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::string pathStr = GetRelativePath(path).string();
		std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
		std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
		
		auto it = m_PathToID.find(pathStr);
		if (it != m_PathToID.end()) {
			auto assetIt = m_Assets.find(it->second);
			if (assetIt != m_Assets.end()) {
				return &assetIt->second;
			}
		}
		return nullptr;
	}

	std::vector<AssetDatabaseEntry> AssetDatabase::GetAssetsByType(AssetType type) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::vector<AssetDatabaseEntry> result;
		for (const auto& [id, entry] : m_Assets) {
			if (entry.Type == type) {
				result.push_back(entry);
			}
		}
		return result;
	}

	std::vector<UUID> AssetDatabase::GetDependencies(UUID assetID) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		auto it = m_Assets.find(assetID);
		if (it != m_Assets.end()) {
			return it->second.Dependencies;
		}
		return {};
	}

	std::vector<UUID> AssetDatabase::GetDependents(UUID assetID) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		std::vector<UUID> dependents;
		for (const auto& [id, entry] : m_Assets) {
			for (UUID dep : entry.Dependencies) {
				if (dep == assetID) {
					dependents.push_back(id);
					break;
				}
			}
		}
		return dependents;
	}

	size_t AssetDatabase::GetAssetCountByType(AssetType type) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		size_t count = 0;
		for (const auto& [id, entry] : m_Assets) {
			if (entry.Type == type) {
				count++;
			}
		}
		return count;
	}

	// ========== FILE WATCHING ==========

	void AssetDatabase::UpdateFileWatchers() {
		std::vector<UUID> modifiedAssets;
		
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			
			for (auto& [id, entry] : m_Assets) {
				std::filesystem::path absolutePath = GetAbsolutePath(entry.RelativePath);
				
				if (std::filesystem::exists(absolutePath)) {
					try {
						auto currentModified = std::filesystem::last_write_time(absolutePath);
						if (currentModified != entry.LastModified) {
							entry.LastModified = currentModified;
							modifiedAssets.push_back(id);
						}
					}
					catch (const std::exception&) {
						// Ignore errors
					}
				}
			}
		}
		
		if (m_OnAssetModified) {
			for (UUID id : modifiedAssets) {
				const AssetDatabaseEntry* entry = GetAssetEntry(id);
				if (entry) {
					m_OnAssetModified(id, GetAbsolutePath(entry->RelativePath));
				}
			}
		}
	}

	// ========== SAVE/LOAD ==========

	bool AssetDatabase::SaveDatabase() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		YAML::Emitter out;
		out << YAML::BeginMap;
		
		out << YAML::Key << "AssetDatabase" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << "1.0";
		out << YAML::Key << "ProjectRoot" << YAML::Value << m_ProjectRoot.string();
		out << YAML::Key << "AssetsFolder" << YAML::Value << m_AssetsFolder.string();
		out << YAML::EndMap;
		
		out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;
		
		for (const auto& [id, entry] : m_Assets) {
			out << YAML::BeginMap;
			out << YAML::Key << "UUID" << YAML::Value << (uint64_t)entry.AssetID;
			out << YAML::Key << "Path" << YAML::Value << entry.RelativePath.string();
			out << YAML::Key << "Type" << YAML::Value << (int)entry.Type;
			out << YAML::Key << "Name" << YAML::Value << entry.Name;
			out << YAML::Key << "FileSize" << YAML::Value << entry.FileSize;
			
			if (!entry.Dependencies.empty()) {
				out << YAML::Key << "Dependencies" << YAML::Value << YAML::BeginSeq;
				for (UUID dep : entry.Dependencies) {
					out << (uint64_t)dep;
				}
				out << YAML::EndSeq;
			}
			
			out << YAML::Key << "HasThumbnail" << YAML::Value << entry.HasThumbnail;
			if (entry.HasThumbnail) {
				out << YAML::Key << "ThumbnailPath" << YAML::Value << entry.ThumbnailPath.string();
			}
			
			out << YAML::EndMap;
		}
		
		out << YAML::EndSeq;
		out << YAML::EndMap;
		
		std::ofstream fout(m_DatabasePath);
		if (!fout.is_open()) {
			LNX_LOG_ERROR("Failed to open AssetDatabase file for writing: {0}", m_DatabasePath.string());
			return false;
		}
		
		fout << out.c_str();
		fout.close();
		
		LNX_LOG_INFO("AssetDatabase saved to {0} ({1} assets)", m_DatabasePath.string(), m_Assets.size());
		return true;
	}

	bool AssetDatabase::LoadDatabase() {
		if (!std::filesystem::exists(m_DatabasePath)) {
			return false;
		}
		
		try {
			YAML::Node data = YAML::LoadFile(m_DatabasePath.string());
			
			if (!data["AssetDatabase"]) {
				LNX_LOG_ERROR("Invalid AssetDatabase file format");
				return false;
			}
			
			auto assets = data["Assets"];
			if (assets && assets.IsSequence()) {
				std::lock_guard<std::mutex> lock(m_Mutex);
				
				m_Assets.clear();
				m_PathToID.clear();
				
				for (const auto& assetNode : assets) {
					AssetDatabaseEntry entry;
					
					entry.AssetID = UUID(assetNode["UUID"].as<uint64_t>());
					entry.RelativePath = assetNode["Path"].as<std::string>();
					entry.Type = (AssetType)assetNode["Type"].as<int>();
					entry.Name = assetNode["Name"].as<std::string>();
					entry.FileSize = assetNode["FileSize"].as<size_t>(0);
					
					std::filesystem::path absolutePath = GetAbsolutePath(entry.RelativePath);
					if (std::filesystem::exists(absolutePath)) {
						entry.LastModified = std::filesystem::last_write_time(absolutePath);
					}
					
					if (assetNode["Dependencies"]) {
						for (const auto& dep : assetNode["Dependencies"]) {
							entry.Dependencies.push_back(UUID(dep.as<uint64_t>()));
						}
					}
					
					entry.HasThumbnail = assetNode["HasThumbnail"].as<bool>(false);
					if (entry.HasThumbnail && assetNode["ThumbnailPath"]) {
						entry.ThumbnailPath = assetNode["ThumbnailPath"].as<std::string>();
					}
					
					RegisterAsset(entry);
				}
			}
			
			return true;
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to load AssetDatabase: {0}", e.what());
			return false;
		}
	}

	// ========== UTILITIES ==========

	AssetType AssetDatabase::GetAssetTypeFromExtension(const std::string& extension) {
		std::string ext = extension;
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		if (ext == ".lumat") return AssetType::Material;
		if (ext == ".lumesh") return AssetType::Mesh;
		if (ext == ".luprefab") return AssetType::Prefab;
		if (ext == ".lunex") return AssetType::Scene;
		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".hdr") 
			return AssetType::Texture;
		if (ext == ".glsl" || ext == ".shader") return AssetType::Shader;
		if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return AssetType::Audio;
		if (ext == ".cpp" || ext == ".h" || ext == ".cs") return AssetType::Script;
		
		return AssetType::None;
	}

	std::string AssetDatabase::GetExtensionForAssetType(AssetType type) {
		switch (type) {
			case AssetType::Material: return ".lumat";
			case AssetType::Mesh: return ".lumesh";
			case AssetType::Prefab: return ".luprefab";
			case AssetType::Scene: return ".lunex";
			case AssetType::Texture: return ".png";
			case AssetType::Shader: return ".glsl";
			case AssetType::Audio: return ".wav";
			case AssetType::Script: return ".cpp";
			default: return "";
		}
	}

	std::filesystem::path AssetDatabase::GetRelativePath(const std::filesystem::path& absolutePath) const {
		try {
			return std::filesystem::relative(absolutePath, m_AssetsFolder);
		}
		catch (...) {
			return absolutePath;
		}
	}

	std::filesystem::path AssetDatabase::GetAbsolutePath(const std::filesystem::path& relativePath) const {
		return m_AssetsFolder / relativePath;
	}

}
