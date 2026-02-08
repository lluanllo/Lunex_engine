/**
 * @file ScriptEntry.h
 * @brief Script Entry Component for Script Component
 */

#pragma once

#include "../UICore.h"
#include "../UIComponents.h"
#include "../UILayout.h"
#include "ScriptVarEditor.h"
#include "../../Lunex-ScriptCore/src/LunexScriptingAPI.h"
#include <vector>

namespace Lunex::UI {

	// ============================================================================
	// SCRIPT ENTRY COMPONENT
	// ============================================================================
	
	struct ScriptEntryResult {
		bool removeClicked = false;
		bool openClicked = false;
		bool varsChanged = false;
	};
	
	/**
	 * @class ScriptEntry
	 * @brief Renders a script entry card in the script component
	 * 
	 * Features:
	 * - Script file name
	 * - Load status indicator
	 * - Remove button
	 * - Public variable editor (when script is loaded)
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
		 * @param publicVars Public variables from reflection (empty if not loaded)
		 * @return Interaction result
		 */
		ScriptEntryResult Render(const std::string& id,
								 const std::string& scriptPath,
								 int index,
								 bool isLoaded,
								 std::vector<Lunex::VarMetadata> publicVars = {});
		
	private:
		void RenderHeader(int index, bool& removeClicked);
		void RenderFileInfo(const std::string& scriptPath);
		void RenderStatus(bool isLoaded);
		bool RenderPublicVars(std::vector<Lunex::VarMetadata>& vars);
		
		ScriptVarEditor m_VarEditor;
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	ScriptEntryResult RenderScriptEntry(const std::string& id,
										const std::string& scriptPath,
										int index,
										bool isLoaded,
										std::vector<Lunex::VarMetadata> publicVars = {});

} // namespace Lunex::UI
