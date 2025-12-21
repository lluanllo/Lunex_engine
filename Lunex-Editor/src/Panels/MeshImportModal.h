#pragma once

#include "Core/Core.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Assets/Mesh/MeshImporter.h"

#include <filesystem>
#include <functional>

namespace Lunex {

	// ============================================================================
	// MESH IMPORT MODAL
	// Modal dialog for configuring mesh import settings when dropping
	// a 3D model file onto the viewport or content browser
	// ============================================================================
	class MeshImportModal {
	public:
		using OnImportCallback = std::function<void(Ref<MeshAsset> meshAsset)>;
		
		MeshImportModal() = default;
		~MeshImportModal() = default;

		// Show the modal for a specific source file
		void Open(const std::filesystem::path& sourcePath, const std::filesystem::path& outputDir = "");
		
		// Close the modal
		void Close();
		
		// Check if modal is open
		bool IsOpen() const { return m_IsOpen; }

		// Render the modal (call every frame)
		void OnImGuiRender();

		// Set callback for when import is complete
		void SetOnImportCallback(OnImportCallback callback) { m_OnImportCallback = callback; }

	private:
		void DrawImportSettings();
		void DrawPreview();
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

}
