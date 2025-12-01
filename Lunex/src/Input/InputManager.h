#pragma once

#include "Core/Core.h"
#include "Input/Action.h"
#include "Input/ActionRegistry.h"
#include "Input/KeyMap.h"
#include "Input/InputConfig.h"
#include "Core/Timestep.h"
#include <unordered_map>

namespace Lunex {

	/**
	 * InputManager - Centralized input handling with action remapping
	 * 
	 * Manages:
	 * - KeyMap for bindings
	 * - ActionRegistry for actions
	 * - Action state tracking (pressed, held, released)
	 * - Config file persistence
	 * 
	 * Singleton pattern for global access.
	 */
	class InputManager {
	public:
		static InputManager& Get() {
			static InputManager instance;
			return instance;
		}

		/**
		 * Initialize the input system
		 * - Loads config file
		 * - Registers default actions
		 */
		void Initialize();

		/**
		 * Shutdown and cleanup
		 */
		void Shutdown();

		/**
		 * Update action states (call every frame)
		 * @param ts Delta time for held duration tracking
		 */
		void Update(Timestep ts);

		/**
		 * Process key press event
		 * @param key Key code
		 * @param modifiers Modifier flags
		 */
		void OnKeyPressed(KeyCode key, uint8_t modifiers);

		/**
		 * Process key release event
		 * @param key Key code
		 * @param modifiers Modifier flags
		 */
		void OnKeyReleased(KeyCode key, uint8_t modifiers);

		/**
		 * Get the KeyMap instance
		 */
		KeyMap& GetKeyMap() { return m_KeyMap; }
		const KeyMap& GetKeyMap() const { return m_KeyMap; }

		/**
		 * Get ActionRegistry instance
		 */
		ActionRegistry& GetRegistry() { return ActionRegistry::Get(); }

		/**
		 * Save bindings to global editor config (assets/InputConfigs)
		 */
		bool SaveBindings();

		/**
		 * Load bindings from global editor config (assets/InputConfigs)
		 */
		bool LoadBindings();

		/**
		 * Reset to default bindings (uses ResetToDefaults then saves)
		 */
		void ResetToDefaults();

		/**
		 * Get config file path
		 */
		std::filesystem::path GetConfigPath() const { return m_ConfigPath; }

		/**
		 * Set config file path
		 */
		void SetConfigPath(const std::filesystem::path& path) { m_ConfigPath = path; }

		/**
		 * Check if an action is currently pressed
		 */
		bool IsActionPressed(const std::string& actionName) const;

		/**
		 * Check if an action was just pressed this frame
		 */
		bool IsActionJustPressed(const std::string& actionName) const;

		/**
		 * Check if an action was just released this frame
		 */
		bool IsActionJustReleased(const std::string& actionName) const;

		/**
		 * Get action state
		 */
		const ActionState* GetActionState(const std::string& actionName) const;

	private:
		InputManager() = default;
		~InputManager() = default;
		InputManager(const InputManager&) = delete;
		InputManager& operator=(const InputManager&) = delete;

		/**
		 * Register default engine actions
		 */
		void RegisterDefaultActions();

		/**
		 * Execute an action with given state
		 */
		void ExecuteAction(const std::string& actionName, const ActionState& state);

	private:
		KeyMap m_KeyMap;
		std::unordered_map<std::string, ActionState> m_ActionStates;
		std::filesystem::path m_ConfigPath = "Config/InputBindings.yaml";
		bool m_Initialized = false;
	};

} // namespace Lunex
