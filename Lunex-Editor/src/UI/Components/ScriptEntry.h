/**
 * @file ScriptEntry.h
 * @brief Script Entry Component for Script Component
 */

#pragma once

#include "../UICore.h"
#include "../UIComponents.h"
#include "../UILayout.h"

namespace Lunex::UI {

	// ============================================================================
	// SCRIPT ENTRY COMPONENT
	// ============================================================================
	
	struct ScriptEntryResult {
		bool removeClicked = false;
		bool openClicked = false;
	};
	
	/**
	 * @class ScriptEntry
	 * @brief Renders a script entry card in the script component
	 * 
	 * Features:
	 * - Script file name
	 * - Load status indicator
	 * - Remove button
	 */
	class ScriptEntry {
	public:
		ScriptEntry() = default;
		~ScriptEntry() = default;
		
		/**
		 * @brief Render the script entry
		 * @param id Unique identifier
		 * @param scriptPath Path to the script file
		 * @param index Script index in the list
		 * @param isLoaded Whether the script is loaded
		 * @return Interaction result
		 */
		ScriptEntryResult Render(const std::string& id,
								 const std::string& scriptPath,
								 int index,
								 bool isLoaded);
		
	private:
		void RenderHeader(int index, bool& removeClicked);
		void RenderFileInfo(const std::string& scriptPath);
		void RenderStatus(bool isLoaded);
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	ScriptEntryResult RenderScriptEntry(const std::string& id,
										const std::string& scriptPath,
										int index,
										bool isLoaded);

} // namespace Lunex::UI
