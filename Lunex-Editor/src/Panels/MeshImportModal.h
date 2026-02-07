/**
 * @file MeshImportModal.h
 * @brief Mesh Import Modal - 3D model import configuration dialog
 * 
 * Features:
 * - Asset name configuration
 * - Transform settings (scale, rotation, translation)
 * - Processing settings (normals, tangents, optimization)
 * - LOD generation settings
 * - Collision generation settings
 * - Model preview information
 */

#pragma once

#include "Core/Core.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Assets/Mesh/MeshImporter.h"

#include <filesystem>
#include <functional>

namespace Lunex {

	class MeshImportModal {
	public:
		using OnImportCallback = std::function<void(Ref<MeshAsset> meshAsset)>;
		
		MeshImportModal() = default;
		~MeshImportModal() = default;

		// Modal control
		void Open(const std::filesystem::path& sourcePath, const std::filesystem::path& outputDir = "");
		void Close();
		bool IsOpen() const { return m_IsOpen; }

		// Render
		void OnImGuiRender();

		// Callback
		void SetOnImportCallback(OnImportCallback callback) { m_OnImportCallback = callback; }

	private:
		// Render sections
		void DrawPreview();
		void DrawImportSettings();
		void DrawActionButtons();
		
		// Import execution
		void DoImport();

	private:
		bool m_IsOpen = false;
		
		// Source file
		std::filesystem::path m_SourcePath;
		std::filesystem::path m_OutputDirectory;
		std::string m_AssetName;
		
		// Import settings
		MeshImportSettings m_ImportSettings;
		
		// Preview info
		MeshImporter::ModelInfo m_ModelInfo;
		bool m_HasPreviewInfo = false;
		
		// Callback
		OnImportCallback m_OnImportCallback;
	};

} // namespace Lunex
