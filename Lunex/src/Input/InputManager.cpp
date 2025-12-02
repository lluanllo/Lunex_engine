#include "stpch.h"
#include "InputManager.h"
#include "InputConfig.h"
#include "Log/Log.h"
#include "Project/Project.h"
#include "Project/ProjectManager.h"

namespace Lunex {

	void InputManager::Initialize() {
		if (m_Initialized) {
			LNX_LOG_WARN("InputManager already initialized");
			return;
		}

		LNX_LOG_INFO("Initializing InputManager...");

		// Register default actions
		RegisterDefaultActions();

		// ? Load from GLOBAL editor config (assets/InputConfigs/EditorInputBindings.yaml)
		// This file is committed to the repository with sensible defaults
		if (!LoadBindings()) {
			LNX_LOG_ERROR("Failed to load global input bindings!");
			LNX_LOG_ERROR("Make sure assets/InputConfigs/EditorInputBindings.yaml exists");
			// Use hardcoded defaults as emergency fallback
			ResetToDefaults();
		}

		m_Initialized = true;
		
		// ? DEBUG: Print all loaded bindings
		LNX_LOG_INFO("=== LOADED INPUT BINDINGS ===");
		for (const auto& [binding, actionName] : m_KeyMap.GetAllBindings()) {
			LNX_LOG_INFO("  Key {0} + Mods {1} -> {2}", 
				static_cast<int>(binding.Key), 
				static_cast<int>(binding.Modifiers), 
				actionName);
		}
		LNX_LOG_INFO("=== END BINDINGS ({0} total) ===", m_KeyMap.GetBindingCount());
		
		LNX_LOG_INFO("InputManager initialized with {0} actions and {1} bindings",
			ActionRegistry::Get().GetActionCount(), m_KeyMap.GetBindingCount());
	}

	void InputManager::Shutdown() {
		if (!m_Initialized)
			return;

		LNX_LOG_INFO("Shutting down InputManager...");

		// ? Save to GLOBAL config on shutdown
		SaveBindings();

		// Clear states
		m_ActionStates.clear();
		m_KeyMap.Clear();

		m_Initialized = false;
	}

	void InputManager::Update(Timestep ts) {
		// Update held durations for pressed actions
		for (auto& [actionName, state] : m_ActionStates) {
			state.WasPressed = state.IsPressed;

			if (state.IsPressed) {
				state.HeldTime += ts;

				// Execute Held context actions
				Action* action = ActionRegistry::Get().GetAction(actionName);
				if (action && action->GetContext() == ActionContext::Held) {
					action->Execute(state);
				}
			}
			else {
				state.HeldTime = 0.0f;
			}
		}
	}

	void InputManager::OnKeyPressed(KeyCode key, uint8_t modifiers) {
		auto actionName = m_KeyMap.GetActionFor(key, modifiers);
		if (!actionName) {
			return;
		}

		// Get or create action state
		ActionState& state = m_ActionStates[*actionName];
		
		// ? Update WasPressed BEFORE setting IsPressed so JustPressed() works correctly
		state.WasPressed = state.IsPressed;
		state.IsPressed = true;

		// Execute action
		ExecuteAction(*actionName, state);
	}

	void InputManager::OnKeyReleased(KeyCode key, uint8_t modifiers) {
		auto actionName = m_KeyMap.GetActionFor(key, modifiers);
		if (!actionName)
			return;

		// Update action state
		ActionState& state = m_ActionStates[*actionName];
		
		// ? FIX: Update WasPressed BEFORE setting IsPressed
		state.WasPressed = state.IsPressed;
		state.IsPressed = false;

		// Execute Released context actions
		Action* action = ActionRegistry::Get().GetAction(*actionName);
		if (action && action->GetContext() == ActionContext::Released) {
			action->Execute(state);
		}
	}

	void InputManager::ExecuteAction(const std::string& actionName, const ActionState& state) {
		Action* action = ActionRegistry::Get().GetAction(actionName);
		if (!action)
			return;

		// Execute based on context
		ActionContext context = action->GetContext();

		if (context == ActionContext::Pressed && state.JustPressed()) {
			action->Execute(state);
		}
		else if (context == ActionContext::Any) {
			action->Execute(state);
		}
		// Held is handled in Update()
	}

	void InputManager::ResetToDefaults() {
		LNX_LOG_INFO("Resetting to default input bindings");

		m_KeyMap.Clear();
		m_ActionStates.clear();

		// Camera Controls - WASD QE for movement
		m_KeyMap.Bind(Key::W, KeyModifiers::None, "Camera.MoveForward");
		m_KeyMap.Bind(Key::S, KeyModifiers::None, "Camera.MoveBackward");
		m_KeyMap.Bind(Key::A, KeyModifiers::None, "Camera.MoveLeft");
		m_KeyMap.Bind(Key::D, KeyModifiers::None, "Camera.MoveRight");
		m_KeyMap.Bind(Key::Q, KeyModifiers::None, "Camera.MoveDown");
		m_KeyMap.Bind(Key::E, KeyModifiers::None, "Camera.MoveUp");

		// Editor Operations - Ctrl+Key
		m_KeyMap.Bind(Key::S, KeyModifiers::Ctrl, "Editor.SaveScene");
		m_KeyMap.Bind(Key::O, KeyModifiers::Ctrl, "Editor.OpenScene");
		m_KeyMap.Bind(Key::N, KeyModifiers::Ctrl, "Editor.NewScene");
		m_KeyMap.Bind(Key::P, KeyModifiers::Ctrl, "Editor.PlayScene");
		m_KeyMap.Bind(Key::D, KeyModifiers::Ctrl, "Editor.DuplicateEntity");

		// Gizmo Operations - Number keys ONLY (no conflicts with camera!)
		m_KeyMap.Bind(Key::D1, KeyModifiers::None, "Gizmo.None");
		m_KeyMap.Bind(Key::D2, KeyModifiers::None, "Gizmo.Translate");
		m_KeyMap.Bind(Key::D3, KeyModifiers::None, "Gizmo.Rotate");
		m_KeyMap.Bind(Key::D4, KeyModifiers::None, "Gizmo.Scale");

		// Debug - Function keys
		m_KeyMap.Bind(Key::F1, KeyModifiers::None, "Debug.ToggleStats");
		m_KeyMap.Bind(Key::F2, KeyModifiers::None, "Debug.ToggleColliders");
		m_KeyMap.Bind(Key::GraveAccent, KeyModifiers::None, "Debug.ToggleConsole");
		
		// Preferences
		m_KeyMap.Bind(Key::K, KeyModifiers::Ctrl, "Preferences.InputSettings");

		// ? Save to GLOBAL config
		SaveBindings();

		LNX_LOG_INFO("Reset to {0} default bindings", m_KeyMap.GetBindingCount());
	}

	bool InputManager::IsActionPressed(const std::string& actionName) const {
		auto it = m_ActionStates.find(actionName);
		return it != m_ActionStates.end() && it->second.IsPressed;
	}

	bool InputManager::IsActionJustPressed(const std::string& actionName) const {
		auto it = m_ActionStates.find(actionName);
		return it != m_ActionStates.end() && it->second.JustPressed();
	}

	bool InputManager::IsActionJustReleased(const std::string& actionName) const {
		auto it = m_ActionStates.find(actionName);
		return it != m_ActionStates.end() && it->second.JustReleased();
	}

	const ActionState* InputManager::GetActionState(const std::string& actionName) const {
		auto it = m_ActionStates.find(actionName);
		return it != m_ActionStates.end() ? &it->second : nullptr;
	}

	void InputManager::RegisterDefaultActions() {
		LNX_LOG_INFO("Registering default engine actions...");

		auto& registry = ActionRegistry::Get();

		// Note: These are placeholder actions that can be overridden by EditorLayer
		// The actual functionality is implemented in EditorLayer callbacks

		// Camera actions (registered but not implemented here - EditorCamera handles them)
		registry.Register("Camera.MoveForward", CreateRef<FunctionAction>(
			"Camera.MoveForward", ActionContext::Held,
			[](const ActionState&) { /* Handled by EditorCamera */ },
			"Move camera forward"
		));

		registry.Register("Camera.MoveBackward", CreateRef<FunctionAction>(
			"Camera.MoveBackward", ActionContext::Held,
			[](const ActionState&) { /* Handled by EditorCamera */ },
			"Move camera backward"
		));

		registry.Register("Camera.MoveLeft", CreateRef<FunctionAction>(
			"Camera.MoveLeft", ActionContext::Held,
			[](const ActionState&) { /* Handled by EditorCamera */ },
			"Move camera left"
		));

		registry.Register("Camera.MoveRight", CreateRef<FunctionAction>(
			"Camera.MoveRight", ActionContext::Held,
			[](const ActionState&) { /* Handled by EditorCamera */ },
			"Move camera right"
		));

		registry.Register("Camera.MoveUp", CreateRef<FunctionAction>(
			"Camera.MoveUp", ActionContext::Held,
			[](const ActionState&) { /* Handled by EditorCamera */ },
			"Move camera up"
		));

		registry.Register("Camera.MoveDown", CreateRef<FunctionAction>(
			"Camera.MoveDown", ActionContext::Held,
			[](const ActionState&) { /* Handled by EditorCamera */ },
			"Move camera down"
		));

		// Editor operations (will be overridden by EditorLayer)
		registry.Register("Editor.SaveScene", CreateRef<FunctionAction>(
			"Editor.SaveScene", ActionContext::Pressed,
			[](const ActionState&) { LNX_LOG_WARN("SaveScene action not implemented"); },
			"Save current scene", true
		));

		registry.Register("Editor.OpenScene", CreateRef<FunctionAction>(
			"Editor.OpenScene", ActionContext::Pressed,
			[](const ActionState&) { LNX_LOG_WARN("OpenScene action not implemented"); },
			"Open scene", true
		));

		registry.Register("Editor.NewScene", CreateRef<FunctionAction>(
			"Editor.NewScene", ActionContext::Pressed,
			[](const ActionState&) { LNX_LOG_WARN("NewScene action not implemented"); },
			"Create new scene", true
		));

		registry.Register("Editor.PlayScene", CreateRef<FunctionAction>(
			"Editor.PlayScene", ActionContext::Pressed,
			[](const ActionState&) { LNX_LOG_WARN("PlayScene action not implemented"); },
			"Play scene", true
		));

		registry.Register("Editor.DuplicateEntity", CreateRef<FunctionAction>(
			"Editor.DuplicateEntity", ActionContext::Pressed,
			[](const ActionState&) { LNX_LOG_WARN("DuplicateEntity action not implemented"); },
			"Duplicate selected entity", true
		));

		// Gizmo operations
		registry.Register("Gizmo.None", CreateRef<FunctionAction>(
			"Gizmo.None", ActionContext::Pressed,
			[](const ActionState&) { /* Handled by EditorLayer */ },
			"Deselect gizmo", false
		));

		registry.Register("Gizmo.Translate", CreateRef<FunctionAction>(
			"Gizmo.Translate", ActionContext::Pressed,
			[](const ActionState&) { /* Handled by EditorLayer */ },
			"Translate gizmo", false
		));

		registry.Register("Gizmo.Rotate", CreateRef<FunctionAction>(
			"Gizmo.Rotate", ActionContext::Pressed,
			[](const ActionState&) { /* Handled by EditorLayer */ },
			"Rotate gizmo", false
		));

		registry.Register("Gizmo.Scale", CreateRef<FunctionAction>(
			"Gizmo.Scale", ActionContext::Pressed,
			[](const ActionState&) { /* Handled by EditorLayer */ },
			"Scale gizmo", false
		));

		// Debug commands
		registry.Register("Debug.ToggleStats", CreateRef<FunctionAction>(
			"Debug.ToggleStats", ActionContext::Pressed,
			[](const ActionState&) { /* Handled by StatsPanel */ },
			"Toggle stats panel"
		));

		registry.Register("Debug.ToggleColliders", CreateRef<FunctionAction>(
			"Debug.ToggleColliders", ActionContext::Pressed,
			[](const ActionState&) { /* Handled by SettingsPanel */ },
			"Toggle collider visualization"
		));

		registry.Register("Debug.ToggleConsole", CreateRef<FunctionAction>(
			"Debug.ToggleConsole", ActionContext::Pressed,
			[](const ActionState&) { /* Handled by ConsolePanel */ },
			"Toggle console panel"
		));
		
		// Preferences
		registry.Register("Preferences.InputSettings", CreateRef<FunctionAction>(
			"Preferences.InputSettings", ActionContext::Pressed,
			[](const ActionState&) { LNX_LOG_WARN("InputSettings action not implemented"); },
			"Open input settings", true
		));

		LNX_LOG_INFO("Registered {0} default actions", registry.GetActionCount());
	}

	// ? Global save/load functions (editor-wide, not per-project)
	bool InputManager::SaveBindings() {
		std::filesystem::path configPath = InputConfig::GetEditorConfigPath();
		
		// Ensure directory exists
		std::filesystem::path configDir = configPath.parent_path();
		if (!std::filesystem::exists(configDir)) {
			std::filesystem::create_directories(configDir);
		}

		if (InputConfig::Save(m_KeyMap, configPath)) {
			LNX_LOG_INFO("Saved {0} global input bindings to: {1}", m_KeyMap.GetBindingCount(), configPath.string());
			return true;
		}
		
		LNX_LOG_ERROR("Failed to save global input bindings");
		return false;
	}

	bool InputManager::LoadBindings() {
		std::filesystem::path configPath = InputConfig::GetEditorConfigPath();
		
		if (!std::filesystem::exists(configPath)) {
			LNX_LOG_WARN("No global input bindings found at: {0}", configPath.string());
			return false;
		}

		m_KeyMap.Clear();
		m_ActionStates.clear();

		if (InputConfig::Load(m_KeyMap, configPath)) {
			LNX_LOG_INFO("Loaded {0} global input bindings from: {1}", m_KeyMap.GetBindingCount(), configPath.string());
			return true;
		}

		LNX_LOG_ERROR("Failed to load global input bindings");
		return false;
	}

} // namespace Lunex