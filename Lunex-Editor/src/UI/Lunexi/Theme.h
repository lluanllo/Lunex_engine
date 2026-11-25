#pragma once

/**
 * @file Theme.h
 * @brief Lunexi themes: named design-token sets applied to ImGui and to UI::Colors.
 *
 * Presets: "Lunex Dark" (default, neutral grays + warm accent), "Lunex Graphite"
 * (lighter, higher contrast), "Midnight" (cool blue-black + cyan) and "Classic Blue"
 * (the original Lunex look). The choice is stored per user in
 * %APPDATA%/Lunex/EditorTheme.txt and restored at startup.
 */

#include "../UICore.h"

#include <string>
#include <vector>

namespace Lunex::UI {

	class Theme {
	public:
		std::string Name;
		ThemeTokens Tokens;

		// Push the tokens into ImGuiStyle (colors + metrics)
		void ApplyToImGui() const;

		static const std::vector<Theme>& GetPresets();
		static const Theme& GetCurrent();
		static bool SetCurrent(const std::string& name, bool persist = true);

		// Load the user's saved choice (falls back to the default preset) and apply it
		static void LoadUserPreference();
	};

} // namespace Lunex::UI
