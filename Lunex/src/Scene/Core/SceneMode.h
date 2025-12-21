#pragma once

/**
 * @file SceneMode.h
 * @brief Scene mode enumeration for AAA architecture
 * 
 * AAA Architecture: SceneMode defines the current state of a scene
 * and determines which systems are active and how they behave.
 */

#include <cstdint>
#include <string>

namespace Lunex {

	/**
	 * @enum SceneMode
	 * @brief Defines the operational mode of a scene
	 */
	enum class SceneMode : uint8_t {
		Edit = 0,      // Editor mode - no physics, no scripts running
		Play = 1,      // Full runtime - physics, scripts, audio all active
		Simulate = 2,  // Physics only - no scripts, useful for testing
		Paused = 3     // Runtime paused - systems frozen but state preserved
	};

	/**
	 * @brief Convert SceneMode to string for debugging/serialization
	 */
	inline const char* SceneModeToString(SceneMode mode) {
		switch (mode) {
			case SceneMode::Edit:     return "Edit";
			case SceneMode::Play:     return "Play";
			case SceneMode::Simulate: return "Simulate";
			case SceneMode::Paused:   return "Paused";
		}
		return "Unknown";
	}

	/**
	 * @brief Parse SceneMode from string
	 */
	inline SceneMode SceneModeFromString(const std::string& str) {
		if (str == "Edit")     return SceneMode::Edit;
		if (str == "Play")     return SceneMode::Play;
		if (str == "Simulate") return SceneMode::Simulate;
		if (str == "Paused")   return SceneMode::Paused;
		return SceneMode::Edit;
	}

	/**
	 * @brief Check if mode involves runtime execution
	 */
	inline bool IsRuntimeMode(SceneMode mode) {
		return mode == SceneMode::Play || mode == SceneMode::Simulate;
	}

	/**
	 * @brief Check if physics should be active in this mode
	 */
	inline bool IsPhysicsActiveInMode(SceneMode mode) {
		return mode == SceneMode::Play || mode == SceneMode::Simulate;
	}

	/**
	 * @brief Check if scripts should be active in this mode
	 */
	inline bool IsScriptActiveInMode(SceneMode mode) {
		return mode == SceneMode::Play;
	}

} // namespace Lunex
