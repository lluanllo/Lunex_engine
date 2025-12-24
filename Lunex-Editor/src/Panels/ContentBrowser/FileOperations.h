#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <functional>

namespace Lunex {

	// ============================================================================
	// FILE OPERATIONS - Handles file creation, deletion, renaming, importing
	// ============================================================================
	class ContentBrowserFileOperations {
	public:
		ContentBrowserFileOperations() = default;
		~ContentBrowserFileOperations() = default;

		// Set base directory for relative path calculations
		void SetBaseDirectory(const std::filesystem::path& baseDir) { m_BaseDirectory = baseDir; }

		// File creation
		void CreateNewFolder(const std::filesystem::path& parentDir, const std::string& name);
		void CreateNewScene(const std::filesystem::path& parentDir);
		void CreateNewScript(const std::filesystem::path& parentDir);
		void CreateNewMaterial(const std::filesystem::path& parentDir);
		void CreatePrefabFromMesh(const std::filesystem::path& meshAssetPath, 
			const std::filesystem::path& outputDir);

		// File operations
		void DeleteItem(const std::filesystem::path& path);
		void RenameItem(const std::filesystem::path& oldPath, const std::string& newName);
		void DuplicateItem(const std::filesystem::path& path);
		void MoveItem(const std::filesystem::path& source, const std::filesystem::path& destination);

		// Import operations
		void ImportFiles(const std::vector<std::string>& files, const std::filesystem::path& targetDir);
		void ImportFilesToFolder(const std::vector<std::string>& files, 
			const std::filesystem::path& targetFolder);

		// Utility
		std::filesystem::path GetUniqueFilePath(const std::filesystem::path& basePath, 
			const std::string& baseName, const std::string& extension);
		std::string GetFileSizeString(uintmax_t size);
		std::string GetLastModifiedString(const std::filesystem::path& path);

		// Callback for thumbnail cache invalidation
		void SetOnThumbnailInvalidate(const std::function<void(const std::filesystem::path&)>& callback) {
			m_OnThumbnailInvalidate = callback;
		}

	private:
		std::filesystem::path m_BaseDirectory;
		std::function<void(const std::filesystem::path&)> m_OnThumbnailInvalidate;
	};

} // namespace Lunex
