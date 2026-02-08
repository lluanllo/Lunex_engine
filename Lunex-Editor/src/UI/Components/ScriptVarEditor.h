/**
 * @file ScriptVarEditor.h
 * @brief Script Variable Editor Component - Renders reflected public variables from scripts
 * 
 * Provides a reusable UI component for editing script public variables
 * exposed via the reflection system (RegisterVar / RegisterVarRange / RegisterVarTooltip).
 * 
 * Uses the LunexUI framework exclusively (no direct ImGui calls).
 */

#pragma once

#include "../UICore.h"
#include "../UIComponents.h"
#include "../../Lunex-ScriptCore/src/LunexScriptingAPI.h"
#include <vector>

namespace Lunex::UI {

	// ============================================================================
	// SCRIPT VAR EDITOR RESULT
	// ============================================================================

	struct ScriptVarEditorResult {
		bool anyChanged = false;
		int changedIndex = -1;
	};

	// ============================================================================
	// SCRIPT VAR EDITOR COMPONENT
	// ============================================================================

	/**
	 * @class ScriptVarEditor
	 * @brief Renders editable controls for reflected script public variables
	 * 
	 * Supports all VarType types:
	 * - Float (DragFloat or Slider if HasRange)
	 * - Int (DragInt)
	 * - Bool (Checkbox)
	 * - Vec2 (DragFloat2)
	 * - Vec3 (DragFloat3)
	 * - String (InputText)
	 * 
	 * Each variable also supports optional tooltips.
	 * 
	 * @example
	 * ```cpp
	 * auto vars = component.GetPublicVariables(scriptIndex);
	 * if (!vars.empty()) {
	 *     ScriptVarEditor editor;
	 *     auto result = editor.Render(vars);
	 *     if (result.anyChanged) {
	 *         // Handle change
	 *     }
	 * }
	 * ```
	 */
	class ScriptVarEditor {
	public:
		ScriptVarEditor() = default;
		~ScriptVarEditor() = default;

		/**
		 * @brief Render all public variables with appropriate controls
		 * @param variables Vector of VarMetadata from the script reflection system
		 * @return Result indicating if any variable was changed
		 */
		ScriptVarEditorResult Render(std::vector<Lunex::VarMetadata>& variables);

	private:
		bool RenderFloatVar(Lunex::VarMetadata& var);
		bool RenderIntVar(Lunex::VarMetadata& var);
		bool RenderBoolVar(Lunex::VarMetadata& var);
		bool RenderVec2Var(Lunex::VarMetadata& var);
		bool RenderVec3Var(Lunex::VarMetadata& var);
		bool RenderStringVar(Lunex::VarMetadata& var);
		void RenderTooltip(const Lunex::VarMetadata& var);
	};

	// ============================================================================
	// STATIC HELPER
	// ============================================================================

	/**
	 * @brief Quick render of script variables
	 */
	ScriptVarEditorResult RenderScriptVarEditor(std::vector<Lunex::VarMetadata>& variables);

} // namespace Lunex::UI
