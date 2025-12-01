#pragma once

#include "Core/Core.h"
#include <string>
#include <functional>

namespace Lunex {

	/**
	 * ActionContext - Defines when an action is triggered
	 */
	enum class ActionContext {
		Pressed,  // Triggered once when key is pressed
		Released, // Triggered once when key is released
		Held,     // Triggered every frame while key is held
		Any       // Triggered for any state change
	};

	/**
	 * ActionState - Current state of an action
	 */
	struct ActionState {
		bool IsPressed = false;
		bool WasPressed = false; // Previous frame state
		float HeldTime = 0.0f;   // How long has been held (seconds)

		bool JustPressed() const { return IsPressed && !WasPressed; }
		bool JustReleased() const { return !IsPressed && WasPressed; }
		bool IsHeld() const { return IsPressed && WasPressed; }
	};

	/**
	 * Action - Base interface for all input actions (Command Pattern)
	 * 
	 * Represents a logical action that can be triggered by input.
	 * Decouples "what key" from "what happens".
	 * 
	 * Example actions: "MoveForward", "Jump", "OpenMenu", "ToggleDebug"
	 */
	class Action {
	public:
		virtual ~Action() = default;

		/**
		 * Execute the action
		 * @param state Current state of the action
		 */
		virtual void Execute(const ActionState& state) = 0;

		/**
		 * Get action name for identification
		 */
		virtual const std::string& GetName() const = 0;

		/**
		 * Get when this action should be triggered
		 */
		virtual ActionContext GetContext() const = 0;

		/**
		 * Can this action be remapped by the user?
		 */
		virtual bool IsRemappable() const { return true; }

		/**
		 * Action description for UI
		 */
		virtual const std::string& GetDescription() const = 0;
	};

	/**
	 * FunctionAction - Simple action using lambda/function
	 * 
	 * Allows quick action creation without subclassing:
	 * 
	 * auto jump = MakeRef<FunctionAction>(
	 *     "Jump", 
	 *     ActionContext::Pressed,
	 *     [](const ActionState& state) { player->Jump(); }
	 * );
	 */
	class FunctionAction : public Action {
	public:
		using ActionFunc = std::function<void(const ActionState&)>;

		FunctionAction(
			const std::string& name,
			ActionContext context,
			ActionFunc func,
			const std::string& description = "",
			bool remappable = true
		)
			: m_Name(name)
			, m_Context(context)
			, m_Function(func)
			, m_Description(description)
			, m_Remappable(remappable)
		{}

		virtual void Execute(const ActionState& state) override {
			if (m_Function) {
				m_Function(state);
			}
		}

		virtual const std::string& GetName() const override { return m_Name; }
		virtual ActionContext GetContext() const override { return m_Context; }
		virtual bool IsRemappable() const override { return m_Remappable; }
		virtual const std::string& GetDescription() const override { return m_Description; }

	private:
		std::string m_Name;
		ActionContext m_Context;
		ActionFunc m_Function;
		std::string m_Description;
		bool m_Remappable;
	};

} // namespace Lunex
