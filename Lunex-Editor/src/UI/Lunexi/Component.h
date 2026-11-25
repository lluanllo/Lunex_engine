#pragma once

/**
 * @file Component.h
 * @brief Lunexi component model (Astro-inspired) on top of the Lunex UI framework.
 *
 * Astro concept        -> Lunexi
 * ------------------------------------------------------------------------------
 * Component + Props    -> Component<TProps>: a typed props struct in, UI out
 * <slot /> / named     -> Slots: Default + named render callbacks the parent fills
 * Layouts              -> Layouts::Panel / Layouts::Split / Layouts::Card
 * Islands (hydration)  -> Island(id, fn): an isolated interactive region whose
 *                         state lives in the framework (UseState) instead of the
 *                         owning panel, keyed by the ImGui ID stack
 * Scoped styles/tokens -> ThemeTokens + Theme (Lunexi/Theme.h), ScopedColor/Style
 *
 * @code
 *   struct BadgeProps { std::string Text; Color Tint = Colors::Primary(); };
 *   class Badge : public UI::Component<BadgeProps> {
 *       void Render(const BadgeProps& p, const UI::Slots&) override { ... }
 *   };
 *
 *   UI::Layouts::Panel({ "Inspector" }, {
 *       .Header  = [&] { UI::Text("Selected: Cube"); },
 *       .Default = [&] { DrawProperties(); },
 *       .Footer  = [&] { if (UI::Button("Apply")) Apply(); },
 *   });
 *
 *   UI::Island("filters", [] {
 *       auto& open = UI::UseState<bool>("open", false);   // persists across frames
 *       UI::PropertyCheckbox("Show filters", open);
 *   });
 * @endcode
 */

#include "../UICore.h"

#include <any>
#include <functional>
#include <string>
#include <unordered_map>

namespace Lunex::UI {

	// ============================================================================
	// SLOTS
	// ============================================================================

	using Slot = std::function<void()>;

	struct Slots {
		Slot Header;
		Slot Default;
		Slot Footer;
		std::unordered_map<std::string, Slot> Named;

		bool Has(const char* name) const { auto it = Named.find(name); return it != Named.end() && it->second; }
		void Render(const char* name) const { auto it = Named.find(name); if (it != Named.end() && it->second) it->second(); }
	};

	// ============================================================================
	// COMPONENT
	// ============================================================================

	template<typename TProps>
	class Component {
	public:
		using Props = TProps;
		virtual ~Component() = default;

		// Renders inside its own ID scope so several instances never collide
		void operator()(const TProps& props, const Slots& slots = {}) {
			ImGui::PushID(this);
			Render(props, slots);
			ImGui::PopID();
		}

	protected:
		virtual void Render(const TProps& props, const Slots& slots) = 0;
	};

	// Stateless function component
	template<typename TProps>
	using FunctionComponent = std::function<void(const TProps&, const Slots&)>;

	// ============================================================================
	// ISLANDS + STATE
	// ============================================================================

	namespace Detail {
		std::unordered_map<ImGuiID, std::any>& StateStore();
	}

	/**
	 * @brief Per-instance UI state keyed by the current ImGui ID stack + key.
	 * Lives as long as the editor (call ClearState to reset an island).
	 */
	template<typename T>
	T& UseState(const char* key, const T& initial = T{}) {
		auto& store = Detail::StateStore();
		ImGuiID id = ImGui::GetID(key);
		auto it = store.find(id);
		if (it == store.end() || !it->second.has_value() || it->second.type() != typeid(T))
			it = store.insert_or_assign(id, std::any(initial)).first;
		return std::any_cast<T&>(it->second);
	}

	void ClearState(const char* key);

	// Isolated interactive region: own ID scope (and state), optional child window
	void Island(const char* id, const Slot& content, const ImVec2& size = ImVec2(0, 0), bool childWindow = false);

	// ============================================================================
	// LAYOUTS
	// ============================================================================

	namespace Layouts {

		struct PanelProps {
			std::string Title;
			float HeaderHeight = 0.0f;   // 0 = auto
			float FooterHeight = 36.0f;
			bool Scrollable = true;
		};
		// Header bar, scrollable body (Default slot) and a pinned footer
		void Panel(const PanelProps& props, const Slots& slots);

		struct SplitProps {
			const char* ID = "split";
			float* Ratio = nullptr;      // shared left/right ratio (0..1), draggable
			float MinLeft = 120.0f;
			float MinRight = 120.0f;
		};
		// Named slots "Left" and "Right" separated by a draggable splitter
		void Split(const SplitProps& props, const Slots& slots);

		struct CardProps {
			std::string Title;
			std::string Subtitle;
			bool Highlight = false;
		};
		// Elevated card with optional title; Default slot is the body
		void Card(const CardProps& props, const Slots& slots);
	}

} // namespace Lunex::UI
