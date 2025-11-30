#pragma once

#include "Core/Core.h"
#include "Core/KeyCodes.h"
#include <string>

namespace Lunex {

	/**
	 * KeyModifiers - Bitflags for modifier keys
	 */
	namespace KeyModifiers {
		constexpr uint8_t None = 0;
		constexpr uint8_t Ctrl = 1 << 0;
		constexpr uint8_t Shift = 1 << 1;
		constexpr uint8_t Alt = 1 << 2;
		constexpr uint8_t Super = 1 << 3;
	}

	/**
	 * Helper function to convert KeyCode to string
	 */
	inline std::string KeyCodeToString(KeyCode key) {
		switch (key) {
			case Key::Space: return "Space";
			case Key::Apostrophe: return "'";
			case Key::Comma: return ",";
			case Key::Minus: return "-";
			case Key::Period: return ".";
			case Key::Slash: return "/";
			case Key::D0: return "0";
			case Key::D1: return "1";
			case Key::D2: return "2";
			case Key::D3: return "3";
			case Key::D4: return "4";
			case Key::D5: return "5";
			case Key::D6: return "6";
			case Key::D7: return "7";
			case Key::D8: return "8";
			case Key::D9: return "9";
			case Key::A: return "A";
			case Key::B: return "B";
			case Key::C: return "C";
			case Key::D: return "D";
			case Key::E: return "E";
			case Key::F: return "F";
			case Key::G: return "G";
			case Key::H: return "H";
			case Key::I: return "I";
			case Key::J: return "J";
			case Key::K: return "K";
			case Key::L: return "L";
			case Key::M: return "M";
			case Key::N: return "N";
			case Key::O: return "O";
			case Key::P: return "P";
			case Key::Q: return "Q";
			case Key::R: return "R";
			case Key::S: return "S";
			case Key::T: return "T";
			case Key::U: return "U";
			case Key::V: return "V";
			case Key::W: return "W";
			case Key::X: return "X";
			case Key::Y: return "Y";
			case Key::Z: return "Z";
			case Key::F1: return "F1";
			case Key::F2: return "F2";
			case Key::F3: return "F3";
			case Key::F4: return "F4";
			case Key::F5: return "F5";
			case Key::F6: return "F6";
			case Key::F7: return "F7";
			case Key::F8: return "F8";
			case Key::F9: return "F9";
			case Key::F10: return "F10";
			case Key::F11: return "F11";
			case Key::F12: return "F12";
			case Key::Escape: return "Esc";
			case Key::Enter: return "Enter";
			case Key::Tab: return "Tab";
			case Key::Backspace: return "Backspace";
			case Key::Insert: return "Insert";
			case Key::Delete: return "Delete";
			case Key::Right: return "Right";
			case Key::Left: return "Left";
			case Key::Down: return "Down";
			case Key::Up: return "Up";
			case Key::PageUp: return "PageUp";
			case Key::PageDown: return "PageDown";
			case Key::Home: return "Home";
			case Key::End: return "End";
			case Key::LeftShift: return "LShift";
			case Key::LeftControl: return "LCtrl";
			case Key::LeftAlt: return "LAlt";
			case Key::RightShift: return "RShift";
			case Key::RightControl: return "RCtrl";
			case Key::RightAlt: return "RAlt";
			case Key::GraveAccent: return "`";
			default: return "Unknown";
		}
	}

	/**
	 * KeyBinding - Represents a key+modifiers bound to an action
	 */
	struct KeyBinding {
		KeyCode Key = Key::Space;
		uint8_t Modifiers = KeyModifiers::None;
		std::string ActionName;

		KeyBinding() = default;
		KeyBinding(KeyCode key, uint8_t mods, const std::string& action)
			: Key(key), Modifiers(mods), ActionName(action) {}

		/**
		 * Generate human-readable string representation
		 * Example: "Ctrl+Shift+S"
		 */
		std::string ToString() const {
			std::string result;

			if (Modifiers & KeyModifiers::Ctrl) result += "Ctrl+";
			if (Modifiers & KeyModifiers::Shift) result += "Shift+";
			if (Modifiers & KeyModifiers::Alt) result += "Alt+";
			if (Modifiers & KeyModifiers::Super) result += "Super+";

			result += KeyCodeToString(Key);
			return result;
		}

		/**
		 * Compare for equality (ignores ActionName)
		 */
		bool operator==(const KeyBinding& other) const {
			return Key == other.Key && Modifiers == other.Modifiers;
		}
	};

	/**
	 * Hash function for KeyBinding (for use in unordered_map)
	 */
	struct KeyBindingHash {
		std::size_t operator()(const KeyBinding& kb) const noexcept {
			return std::hash<uint16_t>()((uint16_t)kb.Key) ^ (std::hash<uint8_t>()(kb.Modifiers) << 1);
		}
	};

} // namespace Lunex
