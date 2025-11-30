#pragma once

#include "Core/Core.h"
#include "KeyBinding.h"
#include "KeyMap.h"
#include <filesystem>

namespace Lunex {

	/**
	 * InputConfig - Serialization for key bindings
	 * 
	 * Saves/loads user key bindings to/from YAML files.
	 * 
	 * File format example:
	 * ```yaml
	 * version: 1
	 * profile: "Default"
	 * bindings:
	 *   - key: W
	 *     modifiers: 0
	 *     action: MoveForward
	 *   - key: S
	 *     modifiers: Ctrl
	 *     action: SaveScene
	 * ```
	 */
	class InputConfig {
	public:
		/**
		 * Save bindings to YAML file
		 */
		static bool Save(const KeyMap& keyMap, const std::filesystem::path& filepath);

		/**
		 * Load bindings from YAML file
		 */
		static bool Load(KeyMap& keyMap, const std::filesystem::path& filepath);

		/**
		 * Create default bindings file
		 */
		static bool CreateDefault(const std::filesystem::path& filepath);

		/**
		 * Get default config file path
		 */
		static std::filesystem::path GetDefaultConfigPath();

		/**
		 * Validate config file version
		 */
		static bool IsCompatible(const std::filesystem::path& filepath);
	};

} // namespace Lunex
