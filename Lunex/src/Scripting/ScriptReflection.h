#pragma once

/**
 * @file ScriptReflection.h
 * @brief Edit-time discovery of a script's public variables (no compilation needed).
 *
 * Scripts expose variables with RegisterVar("Name", &Member) (also RegisterVarRange /
 * RegisterVarTooltip). The source is parsed to find those calls and the member
 * declarations (type + default value), so the inspector can show and edit them in
 * Edit mode, Unity style. Edited values are stored as overrides in ScriptComponent and
 * applied at runtime the moment the script registers the variable.
 */

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace Lunex {

	// Must match Lunex::VarType in Lunex-ScriptCore/src/LunexScriptingAPI.h
	enum class ScriptFieldType : int {
		Float = 0,
		Int = 1,
		Bool = 2,
		Vec2 = 3,
		Vec3 = 4,
		String = 5
	};

	const char* ScriptFieldTypeToString(ScriptFieldType type);
	bool ScriptFieldTypeFromString(const std::string& s, ScriptFieldType& out);

	/**
	 * @struct ScriptFieldValue
	 * @brief Typed value. Numbers live in Number (x for scalars/bools, xy for Vec2, xyz for Vec3)
	 */
	struct ScriptFieldValue {
		ScriptFieldType Type = ScriptFieldType::Float;
		glm::vec4 Number{ 0.0f };
		std::string String;
	};

	struct ScriptFieldInfo {
		std::string Name;
		ScriptFieldValue Default;
		bool HasRange = false;
		float Min = 0.0f;
		float Max = 0.0f;
		std::string Tooltip;
	};

	namespace ScriptReflection {

		/**
		 * @brief Public fields declared by a script (cached by path + modification time)
		 */
		const std::vector<ScriptFieldInfo>& GetFields(const std::filesystem::path& scriptFile);

		/**
		 * @brief Parse source text directly (exposed for tests)
		 */
		std::vector<ScriptFieldInfo> ParseSource(const std::string& source);

	} // namespace ScriptReflection

} // namespace Lunex
