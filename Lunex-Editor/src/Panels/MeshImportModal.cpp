/**
 * @file MeshImportModal.cpp
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

#include "stpch.h"
#include "MeshImportModal.h"
#include "Log/Log.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

#include <imgui.h>

namespace Lunex {

	// ============================================================================
	// MODAL CONTROL
	// ============================================================================

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

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void MeshImportModal::OnImGuiRender() {
		if (!m_IsOpen) return;

		using namespace UI;

		ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
		CenterNextWindow();
		
		if (BeginPanel("Import 3D Model", &m_IsOpen, ImGuiWindowFlags_NoCollapse)) {
			// Header
			{
				ScopedColor headerColor(ImGuiCol_Text, Color(0.85f, 0.85f, 0.85f, 1.0f));
				ImGui::Text("Import: %s", m_SourcePath.filename().string().c_str());
			}
			
			Separator();
			AddSpacing(SpacingValues::SM);

			// Asset name input
			TextStyled("Asset Name:", TextVariant::Secondary);
			char nameBuffer[256];
			strncpy_s(nameBuffer, m_AssetName.c_str(), sizeof(nameBuffer));
			if (InputText("##AssetName", nameBuffer, sizeof(nameBuffer))) {
				m_AssetName = nameBuffer;
			}

			AddSpacing(SpacingValues::SM);

			// Output directory
			TextStyled("Output: " + m_OutputDirectory.string(), TextVariant::Muted);

			AddSpacing(SpacingValues::SM);
			Separator();
			AddSpacing(SpacingValues::SM);

			// Preview info
			DrawPreview();

			AddSpacing(SpacingValues::SM);
			Separator();
			AddSpacing(SpacingValues::SM);

			// Import settings
			DrawImportSettings();

			AddSpacing(SpacingValues::SM);
			Separator();
			AddSpacing(SpacingValues::MD);

			// Action buttons
			DrawActionButtons();
		}
		EndPanel();

		if (!m_IsOpen) {
			Close();
		}
	}

	// ============================================================================
	// PREVIEW SECTION
	// ============================================================================

	void MeshImportModal::DrawPreview() {
		using namespace UI;

		TextStyled("Model Information:", TextVariant::Secondary);
		Indent();

		if (m_HasPreviewInfo) {
			StatItem("Meshes", m_ModelInfo.MeshCount);
			StatItem("Vertices", m_ModelInfo.TotalVertices);
			StatItem("Triangles", m_ModelInfo.TotalTriangles);

			if (m_ModelInfo.HasAnimations) {
				TextColored(Colors::Success(), "? Has Animations");
			}

			if (m_ModelInfo.HasBones) {
				TextColored(Colors::Success(), "? Has Skeleton");
			}

			if (!m_ModelInfo.MaterialNames.empty()) {
				AddSpacing(SpacingValues::XS);
				TextStyled("Materials:", TextVariant::Secondary);
				Indent();
				for (const auto& name : m_ModelInfo.MaterialNames) {
					BulletText(name);
				}
				Unindent();
			}
		}
		else {
			TextColored(Colors::Warning(), "Could not read model info");
		}

		Unindent();
	}

	// ============================================================================
	// IMPORT SETTINGS
	// ============================================================================

	void MeshImportModal::DrawImportSettings() {
		using namespace UI;

		// Transform Settings
		if (ImGui::CollapsingHeader("Transform Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			Indent();
			
			ImGui::DragFloat("Scale", &m_ImportSettings.Scale, 0.01f, 0.001f, 100.0f, "%.3f");
			ImGui::DragFloat3("Rotation", &m_ImportSettings.Rotation.x, 1.0f, -360.0f, 360.0f, "%.1f°");
			ImGui::DragFloat3("Translation", &m_ImportSettings.Translation.x, 0.1f);
			
			Unindent();
		}

		// Processing Settings
		if (ImGui::CollapsingHeader("Processing Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			Indent();
			
			Checkbox("Flip UVs", m_ImportSettings.FlipUVs);
			Checkbox("Generate Normals", m_ImportSettings.GenerateNormals);
			Checkbox("Generate Tangents", m_ImportSettings.GenerateTangents);
			Checkbox("Optimize Mesh", m_ImportSettings.OptimizeMesh);
			
			Unindent();
		}

		// LOD Settings
		if (ImGui::CollapsingHeader("LOD Settings")) {
			Indent();
			
			Checkbox("Generate LODs", m_ImportSettings.GenerateLODs);
			
			if (m_ImportSettings.GenerateLODs) {
				ImGui::SliderInt("LOD Levels", &m_ImportSettings.LODLevels, 1, 6);
				ImGui::SliderFloat("Reduction Factor", &m_ImportSettings.LODReductionFactor, 0.1f, 0.9f, "%.2f");
			}
			
			Unindent();
		}

		// Collision Settings
		if (ImGui::CollapsingHeader("Collision Settings")) {
			Indent();
			
			Checkbox("Generate Collision", m_ImportSettings.GenerateCollision);
			
			if (m_ImportSettings.GenerateCollision) {
				Checkbox("Use Convex Hull", m_ImportSettings.UseConvexCollision);
			}
			
			Unindent();
		}
	}

	// ============================================================================
	// ACTION BUTTONS
	// ============================================================================

	void MeshImportModal::DrawActionButtons() {
		using namespace UI;

		float buttonWidth = 120.0f;
		float windowWidth = ImGui::GetWindowWidth();
		
		// Center buttons
		ImGui::SetCursorPosX((windowWidth - buttonWidth * 2 - 10) / 2);
		
		// Import button
		if (Button("Import", ButtonVariant::Success, ButtonSize::Large, Size(buttonWidth, 35))) {
			DoImport();
		}

		SameLine(0, 10);

		// Cancel button
		if (Button("Cancel", ButtonVariant::Default, ButtonSize::Large, Size(buttonWidth, 35))) {
			Close();
		}
	}

	// ============================================================================
	// IMPORT EXECUTION
	// ============================================================================

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

} // namespace Lunex
