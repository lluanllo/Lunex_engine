#pragma once

#include "Core/Core.h"
#include "KeyMap.h"
#include <filesystem>

namespace Lunex {

	/**
	 * InputConfig - Serialization for input bindings
	 * 
	 * Saves/loads key bindings to/from YAML files.
	 * Supports versioning and validation.
	 */
	class InputConfig {
	public:
		/**
		 * Save key bindings to file
		 */
		static bool Save(const KeyMap& keyMap, const std::filesystem::path& filepath);

		/**
		 * Load key bindings from file
		 */
		static bool Load(KeyMap& keyMap, const std::filesystem::path& filepath);

		/**
		 * Get default config path (relative to project) - DEPRECATED
		 */
		static std::filesystem::path GetDefaultConfigPath();

		/**
		 * Get global editor config path (in editor assets/InputConfigs)
		 */
		static std::filesystem::path GetEditorConfigPath();

		/**
		 * Check if config file is compatible
		 */
		static bool IsCompatible(const std::filesystem::path& filepath);
	};

} // namespace Lunex
