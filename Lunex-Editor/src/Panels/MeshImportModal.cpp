#include "stpch.h"
#include "MeshImportModal.h"
#include "Log/Log.h"

#include <imgui.h>

namespace Lunex {

	void MeshImportModal::Open(const std::filesystem::path& sourcePath, const std::filesystem::path& outputDir) {
		if (!std::filesystem::exists(sourcePath)) {
			LNX_LOG_ERROR("MeshImportModal: Source file not found: {0}", sourcePath.string());
			return;
		}

		if (!MeshImporter::IsSupported(sourcePath)) {
			LNX_LOG_ERROR("MeshImportModal: Unsupported file format: {0}", sourcePath.extension().string());
			return;
		}

		m_SourcePath = sourcePath;
		m_OutputDirectory = outputDir.empty() ? sourcePath.parent_path() : outputDir;
		m_AssetName = sourcePath.stem().string();
		
		// Reset settings to defaults
		m_ImportSettings = MeshImportSettings();
		
		// Get model info for preview
		m_ModelInfo = MeshImporter::GetModelInfo(sourcePath);
		m_HasPreviewInfo = m_ModelInfo.MeshCount > 0;
		
		m_IsOpen = true;
		
		LNX_LOG_INFO("MeshImportModal opened for: {0}", sourcePath.filename().string());
	}

	void MeshImportModal::Close() {
		m_IsOpen = false;
		m_HasPreviewInfo = false;
	}

	void MeshImportModal::OnImGuiRender() {
		if (!m_IsOpen) return;

		ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
		
		if (ImGui::Begin("Import 3D Model", &m_IsOpen, ImGuiWindowFlags_NoCollapse)) {
			// Header
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Import: %s", m_SourcePath.filename().string().c_str());
			ImGui::PopStyleColor();
			
			ImGui::Separator();
			ImGui::Spacing();

			// Asset name
			ImGui::Text("Asset Name:");
			char nameBuffer[256];
			strncpy_s(nameBuffer, m_AssetName.c_str(), sizeof(nameBuffer));
			if (ImGui::InputText("##AssetName", nameBuffer, sizeof(nameBuffer))) {
				m_AssetName = nameBuffer;
			}

			ImGui::Spacing();

			// Output directory
			ImGui::Text("Output: %s", m_OutputDirectory.string().c_str());

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Preview info
			DrawPreview();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Import settings
			DrawImportSettings();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Import button
			float buttonWidth = 120.0f;
			float windowWidth = ImGui::GetWindowWidth();
			
			ImGui::SetCursorPosX((windowWidth - buttonWidth * 2 - 10) / 2);
			
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
			if (ImGui::Button("Import", ImVec2(buttonWidth, 35))) {
				DoImport();
			}
			ImGui::PopStyleColor(2);

			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			if (ImGui::Button("Cancel", ImVec2(buttonWidth, 35))) {
				Close();
			}
			ImGui::PopStyleColor();
		}
		ImGui::End();

		if (!m_IsOpen) {
			Close();
		}
	}

	void MeshImportModal::DrawPreview() {
		ImGui::Text("Model Information:");
		ImGui::Indent();

		if (m_HasPreviewInfo) {
			ImGui::Text("Meshes: %d", m_ModelInfo.MeshCount);
			ImGui::Text("Vertices: %d", m_ModelInfo.TotalVertices);
			ImGui::Text("Triangles: %d", m_ModelInfo.TotalTriangles);

			if (m_ModelInfo.HasAnimations) {
				ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "? Has Animations");
			}

			if (m_ModelInfo.HasBones) {
				ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "? Has Skeleton");
			}

			if (!m_ModelInfo.MaterialNames.empty()) {
				ImGui::Text("Materials:");
				ImGui::Indent();
				for (const auto& name : m_ModelInfo.MaterialNames) {
					ImGui::BulletText("%s", name.c_str());
				}
				ImGui::Unindent();
			}
		}
		else {
			ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "Could not read model info");
		}

		ImGui::Unindent();
	}

	void MeshImportModal::DrawImportSettings() {
		if (ImGui::CollapsingHeader("Transform Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			
			ImGui::DragFloat("Scale", &m_ImportSettings.Scale, 0.01f, 0.001f, 100.0f, "%.3f");
			ImGui::DragFloat3("Rotation", &m_ImportSettings.Rotation.x, 1.0f, -360.0f, 360.0f, "%.1f°");
			ImGui::DragFloat3("Translation", &m_ImportSettings.Translation.x, 0.1f);
			
			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader("Processing Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			
			ImGui::Checkbox("Flip UVs", &m_ImportSettings.FlipUVs);
			ImGui::Checkbox("Generate Normals", &m_ImportSettings.GenerateNormals);
			ImGui::Checkbox("Generate Tangents", &m_ImportSettings.GenerateTangents);
			ImGui::Checkbox("Optimize Mesh", &m_ImportSettings.OptimizeMesh);
			
			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader("LOD Settings")) {
			ImGui::Indent();
			
			ImGui::Checkbox("Generate LODs", &m_ImportSettings.GenerateLODs);
			
			if (m_ImportSettings.GenerateLODs) {
				ImGui::SliderInt("LOD Levels", &m_ImportSettings.LODLevels, 1, 6);
				ImGui::SliderFloat("Reduction Factor", &m_ImportSettings.LODReductionFactor, 0.1f, 0.9f, "%.2f");
			}
			
			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader("Collision Settings")) {
			ImGui::Indent();
			
			ImGui::Checkbox("Generate Collision", &m_ImportSettings.GenerateCollision);
			
			if (m_ImportSettings.GenerateCollision) {
				ImGui::Checkbox("Use Convex Hull", &m_ImportSettings.UseConvexCollision);
			}
			
			ImGui::Unindent();
		}
	}

	void MeshImportModal::DoImport() {
		MeshImportResult result = MeshImporter::ImportAs(
			m_SourcePath,
			m_AssetName,
			m_OutputDirectory,
			m_ImportSettings
		);

		if (result.Success) {
			LNX_LOG_INFO("MeshAsset imported successfully: {0}", result.OutputPath.string());
			
			if (m_OnImportCallback && result.Asset) {
				m_OnImportCallback(result.Asset);
			}
			
			Close();
		}
		else {
			LNX_LOG_ERROR("Failed to import mesh: {0}", result.ErrorMessage);
			// Don't close - let user try again or cancel
		}
	}

}
