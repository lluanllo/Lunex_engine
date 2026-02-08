/**
 * @file ScriptVarEditor.cpp
 * @brief Script Variable Editor Component Implementation
 */

#include "stpch.h"
#include "ScriptVarEditor.h"

namespace Lunex::UI {

	ScriptVarEditorResult ScriptVarEditor::Render(std::vector<Lunex::VarMetadata>& variables) {
		ScriptVarEditorResult result;

		for (size_t i = 0; i < variables.size(); i++) {
			auto& var = variables[i];
			ScopedID varID((int)i);

			bool changed = false;

			switch (var.Type) {
				case Lunex::VarType::Float:
					changed = RenderFloatVar(var);
					break;
				case Lunex::VarType::Int:
					changed = RenderIntVar(var);
					break;
				case Lunex::VarType::Bool:
					changed = RenderBoolVar(var);
					break;
				case Lunex::VarType::Vec2:
					changed = RenderVec2Var(var);
					break;
				case Lunex::VarType::Vec3:
					changed = RenderVec3Var(var);
					break;
				case Lunex::VarType::String:
					changed = RenderStringVar(var);
					break;
			}

			RenderTooltip(var);

			if (changed) {
				result.anyChanged = true;
				result.changedIndex = (int)i;
			}
		}

		return result;
	}

	bool ScriptVarEditor::RenderFloatVar(Lunex::VarMetadata& var) {
		float* ptr = static_cast<float*>(var.DataPtr);
		if (!ptr) return false;

		if (var.HasRange) {
			return PropertySlider(var.Name, *ptr, var.MinValue, var.MaxValue, "%.2f");
		}
		else {
			return PropertyFloat(var.Name, *ptr, 0.1f);
		}
	}

	bool ScriptVarEditor::RenderIntVar(Lunex::VarMetadata& var) {
		int* ptr = static_cast<int*>(var.DataPtr);
		if (!ptr) return false;

		BeginPropertyRow(var.Name);
		bool changed = InputInt("##" + var.Name, *ptr);
		EndPropertyRow();
		return changed;
	}

	bool ScriptVarEditor::RenderBoolVar(Lunex::VarMetadata& var) {
		bool* ptr = static_cast<bool*>(var.DataPtr);
		if (!ptr) return false;

		return PropertyCheckbox(var.Name, *ptr);
	}

	bool ScriptVarEditor::RenderVec2Var(Lunex::VarMetadata& var) {
		Lunex::Vec2* ptr = static_cast<Lunex::Vec2*>(var.DataPtr);
		if (!ptr) return false;

		glm::vec2 value(ptr->x, ptr->y);
		bool changed = PropertyVec2(var.Name, value, 0.1f);
		if (changed) {
			ptr->x = value.x;
			ptr->y = value.y;
		}
		return changed;
	}

	bool ScriptVarEditor::RenderVec3Var(Lunex::VarMetadata& var) {
		Lunex::Vec3* ptr = static_cast<Lunex::Vec3*>(var.DataPtr);
		if (!ptr) return false;

		glm::vec3 value(ptr->x, ptr->y, ptr->z);
		bool changed = PropertyVec3(var.Name, value, 0.1f);
		if (changed) {
			ptr->x = value.x;
			ptr->y = value.y;
			ptr->z = value.z;
		}
		return changed;
	}

	bool ScriptVarEditor::RenderStringVar(Lunex::VarMetadata& var) {
		std::string* ptr = static_cast<std::string*>(var.DataPtr);
		if (!ptr) return false;

		BeginPropertyRow(var.Name);
		bool changed = InputText("##" + var.Name, *ptr);
		EndPropertyRow();
		return changed;
	}

	void ScriptVarEditor::RenderTooltip(const Lunex::VarMetadata& var) {
		if (!var.Tooltip.empty() && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", var.Tooltip.c_str());
		}
	}

	// ============================================================================
	// STATIC HELPER
	// ============================================================================

	ScriptVarEditorResult RenderScriptVarEditor(std::vector<Lunex::VarMetadata>& variables) {
		ScriptVarEditor editor;
		return editor.Render(variables);
	}

} // namespace Lunex::UI
