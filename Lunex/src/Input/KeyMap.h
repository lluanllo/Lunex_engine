#pragma once

#include "Core/Core.h"
#include "KeyBinding.h"
#include "Log/Log.h"
#include <unordered_map>
#include <optional>
#include <vector>

namespace Lunex {

	/**
	 * KeyMap - Manages key-to-action bindings
	 * 
	 * Allows binding/unbinding keys to actions at runtime.
	 * Supports modifier keys (Ctrl, Shift, Alt, Super).
	 * 
	 * Usage:
	 *   KeyMap keyMap;
	 *   keyMap.Bind(Key::W, KeyModifiers::None, "MoveForward");
	 *   keyMap.Bind(Key::S, KeyModifiers::Ctrl, "SaveScene");
	 *   
	 *   auto action = keyMap.GetActionFor(Key::W, KeyModifiers::None);
	 *   if (action) { ... }
	 */
	class KeyMap {
	public:
		KeyMap() = default;

		/**
		 * Bind a key+modifiers to an action
		 * @param key Key code
		 * @param modifiers Modifier mask
		 * @param actionName Action identifier
		 * @return true if bound, false if already bound to another action
		 */
		bool Bind(KeyCode key, uint8_t modifiers, const std::string& actionName) {
			KeyBinding binding(key, modifiers, actionName);

			// Check if already bound
			if (m_Bindings.find(binding) != m_Bindings.end()) {
				LNX_LOG_WARN("Key {0} already bound to {1}", 
					binding.ToString(), m_Bindings[binding]);
				return false;
			}

			m_Bindings[binding] = actionName;
			LNX_LOG_INFO("Bound {0} to action '{1}'", binding.ToString(), actionName);
			return true;
		}

		/**
		 * Unbind a key+modifiers
		 */
		void Unbind(KeyCode key, uint8_t modifiers) {
			KeyBinding binding(key, modifiers, "");
			auto it = m_Bindings.find(binding);
			
			if (it != m_Bindings.end()) {
				LNX_LOG_INFO("Unbound {0} from action '{1}'", 
					binding.ToString(), it->second);
				m_Bindings.erase(it);
			}
		}

		/**
		 * Unbind all keys for a specific action
		 */
		void UnbindAction(const std::string& actionName) {
			std::vector<KeyBinding> toRemove;

			for (const auto& [binding, action] : m_Bindings) {
				if (action == actionName) {
					toRemove.push_back(binding);
				}
			}

			for (const auto& binding : toRemove) {
				m_Bindings.erase(binding);
			}

			if (!toRemove.empty()) {
				LNX_LOG_INFO("Unbound {0} keys from action '{1}'", 
					toRemove.size(), actionName);
			}
		}

		/**
		 * Get action name for a key+modifiers
		 * @return Action name if bound, nullopt otherwise
		 */
		std::optional<std::string> GetActionFor(KeyCode key, uint8_t modifiers) const {
			KeyBinding binding(key, modifiers, "");
			auto it = m_Bindings.find(binding);
			
			if (it != m_Bindings.end()) {
				return it->second;
			}
			
			return std::nullopt;
		}

		/**
		 * Get all bindings for an action
		 */
		std::vector<KeyBinding> GetBindingsFor(const std::string& actionName) const {
			std::vector<KeyBinding> result;

			for (const auto& [binding, action] : m_Bindings) {
				if (action == actionName) {
					result.push_back(binding);
				}
			}

			return result;
		}

		/**
		 * Check if a key+modifiers is bound
		 */
		bool IsBound(KeyCode key, uint8_t modifiers) const {
			KeyBinding binding(key, modifiers, "");
			return m_Bindings.find(binding) != m_Bindings.end();
		}

		/**
		 * Get all bindings
		 */
		const std::unordered_map<KeyBinding, std::string, KeyBindingHash>& GetAllBindings() const {
			return m_Bindings;
		}

		/**
		 * Clear all bindings
		 */
		void Clear() {
			m_Bindings.clear();
			LNX_LOG_INFO("Cleared all key bindings");
		}

		/**
		 * Get binding count
		 */
		size_t GetBindingCount() const {
			return m_Bindings.size();
		}

		/**
		 * Rebind an action from old key to new key
		 * @return true if successful
		 */
		bool Rebind(KeyCode oldKey, uint8_t oldModifiers, 
		            KeyCode newKey, uint8_t newModifiers) {
			KeyBinding oldBinding(oldKey, oldModifiers, "");
			auto it = m_Bindings.find(oldBinding);

			if (it == m_Bindings.end()) {
				LNX_LOG_WARN("Cannot rebind: {0} not bound", oldBinding.ToString());
				return false;
			}

			std::string actionName = it->second;
			m_Bindings.erase(it);

			return Bind(newKey, newModifiers, actionName);
		}

	private:
		std::unordered_map<KeyBinding, std::string, KeyBindingHash> m_Bindings;
	};

} // namespace Lunex
