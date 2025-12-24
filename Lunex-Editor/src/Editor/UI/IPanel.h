#pragma once

#include <string>

namespace Lunex {
	class Event;

	// ============================================================================
	// IPANEL - Base interface for all editor panels
	// Provides a consistent API for the editor to manage panels
	// ============================================================================
	class IPanel {
	public:
		virtual ~IPanel() = default;

		// Lifecycle
		virtual void OnAttach() {}
		virtual void OnDetach() {}

		// Rendering
		virtual void OnImGuiRender() = 0;

		// Events
		virtual void OnEvent(Event& e) {}

		// Panel state
		virtual const std::string& GetName() const = 0;
		virtual bool IsVisible() const { return m_Visible; }
		virtual void SetVisible(bool visible) { m_Visible = visible; }
		virtual void ToggleVisibility() { m_Visible = !m_Visible; }

		// Focus
		virtual bool IsFocused() const { return m_Focused; }
		virtual void SetFocused(bool focused) { m_Focused = focused; }

	protected:
		bool m_Visible = true;
		bool m_Focused = false;
	};

} // namespace Lunex
