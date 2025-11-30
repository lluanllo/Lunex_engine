#pragma once

#include "Core/Core.h"
#include "Action.h"
#include "Log/Log.h"
#include <unordered_map>
#include <string>

namespace Lunex {

	/**
	 * ActionRegistry - Central registry for all input actions
	 * 
	 * Manages lifecycle of actions and provides lookup by name.
	 * Singleton pattern for global access.
	 * 
	 * Usage:
	 *   ActionRegistry::Get().Register("Jump", jumpAction);
	 *   Action* action = ActionRegistry::Get().GetAction("Jump");
	 */
	class ActionRegistry {
	public:
		static ActionRegistry& Get() {
			static ActionRegistry instance;
			return instance;
		}

		/**
		 * Register a new action
		 * @param name Unique action identifier
		 * @param action Action instance (takes ownership)
		 * @return true if registered, false if name already exists
		 */
		bool Register(const std::string& name, Ref<Action> action) {
			if (m_Actions.find(name) != m_Actions.end()) {
				LNX_LOG_WARN("Action '{0}' already registered", name);
				return false;
			}

			m_Actions[name] = action;
			LNX_LOG_INFO("Registered action: {0}", name);
			return true;
		}

		/**
		 * Unregister an action
		 */
		void Unregister(const std::string& name) {
			auto it = m_Actions.find(name);
			if (it != m_Actions.end()) {
				m_Actions.erase(it);
				LNX_LOG_INFO("Unregistered action: {0}", name);
			}
		}

		/**
		 * Get action by name
		 * @return Action pointer or nullptr if not found
		 */
		Action* GetAction(const std::string& name) {
			auto it = m_Actions.find(name);
			return (it != m_Actions.end()) ? it->second.get() : nullptr;
		}

		/**
		 * Check if action exists
		 */
		bool HasAction(const std::string& name) const {
			return m_Actions.find(name) != m_Actions.end();
		}

		/**
		 * Get all registered action names
		 */
		std::vector<std::string> GetAllActionNames() const {
			std::vector<std::string> names;
			names.reserve(m_Actions.size());
			for (const auto& [name, action] : m_Actions) {
				names.push_back(name);
			}
			return names;
		}

		/**
		 * Get all actions
		 */
		const std::unordered_map<std::string, Ref<Action>>& GetAllActions() const {
			return m_Actions;
		}

		/**
		 * Clear all actions
		 */
		void Clear() {
			m_Actions.clear();
			LNX_LOG_INFO("Cleared all actions");
		}

		/**
		 * Get action count
		 */
		size_t GetActionCount() const {
			return m_Actions.size();
		}

	private:
		ActionRegistry() = default;
		~ActionRegistry() = default;
		ActionRegistry(const ActionRegistry&) = delete;
		ActionRegistry& operator=(const ActionRegistry&) = delete;

	private:
		std::unordered_map<std::string, Ref<Action>> m_Actions;
	};

} // namespace Lunex
