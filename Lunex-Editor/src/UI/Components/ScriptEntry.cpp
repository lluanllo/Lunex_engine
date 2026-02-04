/**
 * @file ScriptEntry.cpp
 * @brief Script Entry Component Implementation
 */

#include "stpch.h"
#include "ScriptEntry.h"
#include <filesystem>

namespace Lunex::UI {

	ScriptEntryResult ScriptEntry::Render(const std::string& id, const std::string& scriptPath,
										  int index, bool isLoaded) {
		ScriptEntryResult result;
		
		ScopedID scopedID(id);
		
		CardStyle cardStyle;
		cardStyle.backgroundColor = Colors::BgDark();
		cardStyle.padding = SpacingValues::SM;
		
		if (BeginCard(id + "_card", Size(-1, 100), cardStyle)) {
			RenderHeader(index, result.removeClicked);
			
			Separator();
			AddSpacing(SpacingValues::SM);
			
			RenderFileInfo(scriptPath);
			
			AddSpacing(SpacingValues::SM);
			
			RenderStatus(isLoaded);
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
		TextStyled("??", TextVariant::Primary);
		SameLine();
		TextWrapped(path.filename().string(), TextVariant::Default);
	}
	
	void ScriptEntry::RenderStatus(bool isLoaded) {
		TextStyled("Status:", TextVariant::Secondary);
		SameLine();
		
		if (isLoaded) {
			TextStyled("? Loaded", TextVariant::Success);
		} else {
			TextStyled("? Will compile on Play", TextVariant::Warning);
		}
	}
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	ScriptEntryResult RenderScriptEntry(const std::string& id, const std::string& scriptPath,
										int index, bool isLoaded) {
		ScriptEntry entry;
		return entry.Render(id, scriptPath, index, isLoaded);
	}

} // namespace Lunex::UI
