#include "stpch.h"
#include "FileOperations.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Assets/Mesh/MeshAsset.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

namespace Lunex {

	void ContentBrowserFileOperations::CreateNewFolder(const std::filesystem::path& parentDir, 
		const std::string& name) {
		std::filesystem::path newFolderPath = parentDir / name;
		if (!std::filesystem::exists(newFolderPath)) {
			std::filesystem::create_directory(newFolderPath);
			LNX_LOG_INFO("Created folder: {0}", newFolderPath.string());
		}
		else {
			LNX_LOG_WARN("Folder already exists: {0}", name);
		}
	}

	void ContentBrowserFileOperations::CreateNewScene(const std::filesystem::path& parentDir) {
		std::filesystem::path scenePath = GetUniqueFilePath(parentDir, "NewScene", ".lunex");

		std::string sceneContent = R"(Scene: NewScene
Entities:
)";

		std::ofstream sceneFile(scenePath);
		sceneFile << sceneContent;
		sceneFile.close();

		LNX_LOG_INFO("Created new scene: {0}", scenePath.string());
	}

	void ContentBrowserFileOperations::CreateNewScript(const std::filesystem::path& parentDir) {
		std::string baseName = "NewScript";
		int counter = 1;
		std::filesystem::path scriptPath;
		
		do {
			baseName = "NewScript" + (counter > 1 ? std::to_string(counter) : "");
			scriptPath = parentDir / (baseName + ".cpp");
			counter++;
		} while (std::filesystem::exists(scriptPath));

		std::string scriptContent = R"(#include "../../Lunex-ScriptCore/src/LunexScriptingAPI.h"
#include <iostream>

namespace Lunex {

    class )" + baseName + R"( : public IScriptModule
    {
    public:
        )" + baseName + R"(() = default;
        ~)" + baseName + R"(() override = default;

        void OnLoad(EngineContext* context) override
        {
            m_Context = context;
            
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Script loaded!");
            }
        }

        void OnUnload() override
        {
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Script unloading...");
            }
            m_Context = nullptr;
        }

        void OnUpdate(float deltaTime) override
        {
            // Your gameplay logic here
        }

        void OnRender() override {}

        void OnPlayModeEnter() override
        {
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Entering Play Mode!");
            }
        }

        void OnPlayModeExit() override
        {
            if (m_Context && m_Context->LogInfo)
            {
                m_Context->LogInfo("[)" + baseName + R"(] Exiting Play Mode!");
            }
        }

    private:
        EngineContext* m_Context = nullptr;
    };

} // namespace Lunex

extern "C"
{
    LUNEX_API uint32_t Lunex_GetScriptingAPIVersion()
    {
        return Lunex::SCRIPTING_API_VERSION;
    }

    LUNEX_API Lunex::IScriptModule* Lunex_CreateModule()
    {
        return new Lunex::)" + baseName + R"(();
    }

    LUNEX_API void Lunex_DestroyModule(Lunex::IScriptModule* module)
    {
        delete module;
    }
}
)";

		std::ofstream scriptFile(scriptPath);
		scriptFile << scriptContent;
		scriptFile.close();

		LNX_LOG_INFO("Created new script: {0}", scriptPath.string());
	}

	void ContentBrowserFileOperations::CreateNewMaterial(const std::filesystem::path& parentDir) {
		std::string baseName = "NewMaterial";
		int counter = 1;
		std::filesystem::path materialPath;

		do {
			baseName = "NewMaterial" + (counter > 1 ? std::to_string(counter) : "");
			materialPath = parentDir / (baseName + ".lumat");
			counter++;
		} while (std::filesystem::exists(materialPath));

		UUID materialID = UUID();

		std::stringstream ss;
		ss << "Material:\n";
		ss << "  ID: " << (uint64_t)materialID << "\n";
		ss << "  Name: " << baseName << "\n";
		ss << "Properties:\n";
		ss << "  Albedo: [1, 1, 1, 1]\n";
		ss << "  Metallic: 0\n";
		ss << "  Roughness: 0.5\n";
		ss << "  Specular: 0.5\n";
		ss << "  EmissionColor: [0, 0, 0]\n";
		ss << "  EmissionIntensity: 0\n";
		ss << "  NormalIntensity: 1\n";
		ss << "Textures:\n";
		ss << "Multipliers:\n";
		ss << "  Metallic: 1\n";
		ss << "  Roughness: 1\n";
		ss << "  Specular: 1\n";
		ss << "  AO: 1\n";

		std::ofstream materialFile(materialPath);
		materialFile << ss.str();
		materialFile.close();

		LNX_LOG_INFO("Created new material: {0}", materialPath.string());
	}

	void ContentBrowserFileOperations::CreatePrefabFromMesh(const std::filesystem::path& meshAssetPath,
		const std::filesystem::path& outputDir) {
		auto meshAsset = MeshAsset::LoadFromFile(meshAssetPath);
		if (!meshAsset) {
			LNX_LOG_ERROR("Failed to load mesh asset: {0}", meshAssetPath.string());
			return;
		}

		std::string baseName = meshAssetPath.stem().string();
		std::filesystem::path prefabsFolder = outputDir / "Prefabs";

		if (!std::filesystem::exists(prefabsFolder)) {
			std::filesystem::create_directories(prefabsFolder);
		}

		std::filesystem::path prefabPath = GetUniqueFilePath(prefabsFolder, baseName, ".luprefab");
		std::string prefabName = prefabPath.stem().string();

		UUID prefabID = UUID();
		UUID entityID = UUID();

		// Get relative path to mesh asset
		auto relativeMeshPath = std::filesystem::relative(meshAssetPath, m_BaseDirectory);
		std::string relativeMeshPathStr = relativeMeshPath.string();
		std::replace(relativeMeshPathStr.begin(), relativeMeshPathStr.end(), '\\', '/');

		std::stringstream ss;
		ss << "Prefab:\n";
		ss << "  Name: " << prefabName << "\n";
		ss << "  Description: Prefab created from mesh " << baseName << "\n";
		ss << "  RootEntityID: " << (uint64_t)entityID << "\n";
		ss << "  UUID: " << (uint64_t)prefabID << "\n";
		ss << "  OriginalTransform:\n";
		ss << "    Position: [0, 0, 0]\n";
		ss << "    Rotation: [0, 0, 0]\n";
		ss << "    Scale: [1, 1, 1]\n";
		ss << "Entities:\n";
		ss << "  - EntityID: " << (uint64_t)entityID << "\n";
		ss << "    Tag: " << prefabName << "\n";
		ss << "    LocalParentID: 0\n";
		ss << "    LocalChildIDs: []\n";
		ss << "    Components:\n";
		ss << "      - Type: TransformComponent\n";
		ss << "        Data: \"0,0,0;0,0,0;1,1,1\"\n";
		ss << "      - Type: MeshComponent\n";
		ss << "        Data: \"4;1,1,1,1;" << (uint64_t)meshAsset->GetID() << ";" << relativeMeshPathStr << ";\"\n";
		ss << "      - Type: MaterialComponent\n";
		ss << "        Data: \"0;;0;1,1,1,1;0;0.5;0.5;0,0,0;0\"\n";

		std::ofstream prefabFile(prefabPath);
		if (!prefabFile) {
			LNX_LOG_ERROR("Failed to create prefab file: {0}", prefabPath.string());
			return;
		}
		prefabFile << ss.str();
		prefabFile.close();

		LNX_LOG_INFO("Created prefab '{0}' from mesh '{1}'", prefabName, baseName);
	}

	void ContentBrowserFileOperations::DeleteItem(const std::filesystem::path& path) {
		try {
			if (std::filesystem::is_directory(path)) {
				std::filesystem::remove_all(path);
				LNX_LOG_INFO("Deleted folder: {0}", path.string());
			}
			else {
				std::filesystem::remove(path);
				LNX_LOG_INFO("Deleted file: {0}", path.string());
			}

			if (m_OnThumbnailInvalidate) {
				m_OnThumbnailInvalidate(path);
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to delete {0}: {1}", path.string(), e.what());
		}
	}

	void ContentBrowserFileOperations::RenameItem(const std::filesystem::path& oldPath,
		const std::string& newName) {
		try {
			std::filesystem::path newPath = oldPath.parent_path() / newName;

			if (std::filesystem::exists(newPath)) {
				LNX_LOG_WARN("Item with name {0} already exists", newName);
				return;
			}

			std::string oldExtension = oldPath.extension().string();
			std::transform(oldExtension.begin(), oldExtension.end(), oldExtension.begin(), ::tolower);

			if (oldExtension == ".lumat") {
				auto material = MaterialRegistry::Get().LoadMaterial(oldPath);
				if (material) {
					std::filesystem::path newNamePath(newName);
					std::string newMaterialName = newNamePath.stem().string();

					material->SetName(newMaterialName);
					material->SetPath(newPath);

					if (!material->SaveToFile(newPath)) {
						LNX_LOG_ERROR("Failed to save material with new name: {0}", newName);
						return;
					}

					std::filesystem::remove(oldPath);
					MaterialRegistry::Get().ReloadMaterial(material->GetID());

					LNX_LOG_INFO("Renamed material {0} to {1}", oldPath.filename().string(), newMaterialName);
				}
				else {
					LNX_LOG_ERROR("Failed to load material for renaming: {0}", oldPath.string());
					return;
				}
			}
			else {
				std::filesystem::rename(oldPath, newPath);
				LNX_LOG_INFO("Renamed {0} to {1}", oldPath.filename().string(), newName);
			}

			if (m_OnThumbnailInvalidate) {
				m_OnThumbnailInvalidate(oldPath);
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to rename {0}: {1}", oldPath.string(), e.what());
		}
	}

	void ContentBrowserFileOperations::DuplicateItem(const std::filesystem::path& path) {
		try {
			std::string baseName = path.stem().string();
			std::string extension = path.extension().string();
			std::filesystem::path destPath = GetUniqueFilePath(path.parent_path(), 
				baseName + " - Copy", extension);

			if (std::filesystem::is_directory(path)) {
				std::filesystem::copy(path, destPath, std::filesystem::copy_options::recursive);
			}
			else {
				std::filesystem::copy_file(path, destPath);
			}

			LNX_LOG_INFO("Duplicated {0} as {1}", path.filename().string(), destPath.filename().string());
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to duplicate {0}: {1}", path.filename().string(), e.what());
		}
	}

	void ContentBrowserFileOperations::MoveItem(const std::filesystem::path& source,
		const std::filesystem::path& destination) {
		try {
			std::filesystem::path destPath = destination / source.filename();

			if (source != destPath && !std::filesystem::exists(destPath)) {
				std::filesystem::rename(source, destPath);
				LNX_LOG_INFO("Moved {0} to {1}", source.filename().string(), 
					destination.filename().string());
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to move {0}: {1}", source.filename().string(), e.what());
		}
	}

	void ContentBrowserFileOperations::ImportFiles(const std::vector<std::string>& files,
		const std::filesystem::path& targetDir) {
		for (const auto& file : files) {
			try {
				std::filesystem::path sourcePath(file);
				std::filesystem::path destPath = targetDir / sourcePath.filename();

				if (std::filesystem::exists(destPath)) {
					LNX_LOG_WARN("File {0} already exists in destination", sourcePath.filename().string());
					continue;
				}

				std::filesystem::copy(sourcePath, destPath);
				LNX_LOG_INFO("Imported file: {0}", sourcePath.filename().string());
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to import {0}: {1}", file, e.what());
			}
		}
	}

	void ContentBrowserFileOperations::ImportFilesToFolder(const std::vector<std::string>& files,
		const std::filesystem::path& targetFolder) {
		for (const auto& file : files) {
			try {
				std::filesystem::path sourcePath(file);
				std::filesystem::path destPath = targetFolder / sourcePath.filename();

				if (std::filesystem::exists(destPath)) {
					LNX_LOG_WARN("File {0} already exists in {1}", 
						sourcePath.filename().string(), targetFolder.filename().string());
					continue;
				}

				std::filesystem::copy(sourcePath, destPath);
				LNX_LOG_INFO("Imported {0} to folder {1}", 
					sourcePath.filename().string(), targetFolder.filename().string());
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to import {0}: {1}", file, e.what());
			}
		}
	}

	std::filesystem::path ContentBrowserFileOperations::GetUniqueFilePath(
		const std::filesystem::path& basePath, const std::string& baseName, 
		const std::string& extension) {
		int counter = 1;
		std::filesystem::path resultPath;

		do {
			std::string name = baseName + (counter > 1 ? std::to_string(counter) : "") + extension;
			resultPath = basePath / name;
			counter++;
		} while (std::filesystem::exists(resultPath));

		return resultPath;
	}

	std::string ContentBrowserFileOperations::GetFileSizeString(uintmax_t size) {
		const char* units[] = { "B", "KB", "MB", "GB", "TB" };
		int unitIndex = 0;
		double displaySize = (double)size;

		while (displaySize >= 1024.0 && unitIndex < 4) {
			displaySize /= 1024.0;
			unitIndex++;
		}

		char buffer[64];
		if (unitIndex == 0) {
			snprintf(buffer, sizeof(buffer), "%d %s", (int)displaySize, units[unitIndex]);
		}
		else {
			snprintf(buffer, sizeof(buffer), "%.2f %s", displaySize, units[unitIndex]);
		}

		return std::string(buffer);
	}

	std::string ContentBrowserFileOperations::GetLastModifiedString(const std::filesystem::path& path) {
		auto ftime = std::filesystem::last_write_time(path);
		auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
			ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
		);
		std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

		char buffer[64];
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", std::localtime(&cftime));

		return std::string(buffer);
	}

} // namespace Lunex
