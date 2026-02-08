/**
 * @file ScriptEntry.cpp
 * @brief Script Entry Component Implementation
 */

#include "stpch.h"
#include "ScriptEntry.h"
#include "ComponentDrawer.h"
#include <filesystem>

namespace Lunex::UI {

	ScriptEntryResult ScriptEntry::Render(const std::string& id, const std::string& scriptPath,
										  int index, bool isLoaded,
										  std::vector<Lunex::VarMetadata> publicVars) {
		ScriptEntryResult result;
		
		ScopedID scopedID(id);
		
		// Calculate dynamic height based on content
		float baseHeight = 100.0f;
		if (isLoaded && !publicVars.empty()) {
			baseHeight += 30.0f + (float)publicVars.size() * 28.0f;
		}
		
		CardStyle cardStyle;
		cardStyle.backgroundColor = Colors::BgDark();
		cardStyle.padding = SpacingValues::SM;
		
		if (BeginCard(id + "_card", Size(-1, baseHeight), cardStyle)) {
			RenderHeader(index, result.removeClicked);
			
			Separator();
			AddSpacing(SpacingValues::SM);
			
			RenderFileInfo(scriptPath);
			
			AddSpacing(SpacingValues::SM);
			
			RenderStatus(isLoaded);
			
			// Render public variables if the script is loaded and has them
			if (isLoaded && !publicVars.empty()) {
				result.varsChanged = RenderPublicVars(publicVars);
			}
		}
		EndCard();
		
		return result;
	}
	
	void ScriptEntry::RenderHeader(int index, bool& removeClicked) {
		TextStyled("Script #" + std::to_string(index + 1), TextVariant::Muted);
		
		SameLine(ImGui::GetContentRegionAvail().x - 65);
		
		if (Button("Remove", ButtonVariant::Danger, ButtonSize::Small, Size(65, 0))) {
			removeClicked = true;
		}
	}
	
	void ScriptEntry::RenderFileInfo(const std::string& scriptPath) {
		std::filesystem::path path(scriptPath);
		{
			ScopedColor textColor(ImGuiCol_Text, ComponentStyle::AccentColor());
			TextStyled("File:", TextVariant::Primary);
		}
		SameLine();
		TextWrapped(path.filename().string(), TextVariant::Default);
	}
	
	void ScriptEntry::RenderStatus(bool isLoaded) {
		TextStyled("Status:", TextVariant::Secondary);
		SameLine();
		
		if (isLoaded) {
			TextStyled("Loaded", TextVariant::Success);
		} else {
			TextStyled("Will compile on Play", TextVariant::Warning);
		}
	}
	
	bool ScriptEntry::RenderPublicVars(std::vector<Lunex::VarMetadata>& vars) {
		AddSpacing(SpacingValues::XS);
		Separator();
		AddSpacing(SpacingValues::XS);
		
		{
			ScopedColor textColor(ImGuiCol_Text, ComponentStyle::SubheaderColor());
			TextStyled("Public Variables", TextVariant::Secondary);
		}
		AddSpacing(SpacingValues::XS);
		
		auto editorResult = m_VarEditor.Render(vars);
		return editorResult.anyChanged;
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	ScriptEntryResult RenderScriptEntry(const std::string& id, const std::string& scriptPath,
										int index, bool isLoaded,
										std::vector<Lunex::VarMetadata> publicVars) {
		ScriptEntry entry;
		return entry.Render(id, scriptPath, index, isLoaded, std::move(publicVars));
	}

} // namespace Lunex::UI
