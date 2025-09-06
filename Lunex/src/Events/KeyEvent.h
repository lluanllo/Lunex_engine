#pragma once

#include "../Core/Core.h"
#include "Event.h"

namespace Lunex {
		class LUNEX_API KeyEvent : public Event {
			public:
				inline int GetKeyCode() const { return m_KeyCode; }

				EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
			protected:
				KeyEvent(int keyCode)
					: m_KeyCode(keyCode) {
				}
				int m_KeyCode;
	};

	class LUNEX_API KeyPressedEvent : public KeyEvent {
		public:
			KeyPressedEvent(int keyCode, int repeatCount)
				: KeyEvent(keyCode), m_RepeatCount(repeatCount) {
			}

			inline int GetRepeatCount() const { return m_RepeatCount; }
			std::string ToString() const override {
				std::stringstream ss;
				ss << "KeyPressedEvent: " << m_KeyCode << " (repeat count: " << m_RepeatCount << ")";
				return ss.str();
			}

			EVENT_CLASS_TYPE(KeyPressed)
		private:
			int m_RepeatCount;
	};

	class LUNEX_API KeyReleasedEvent : public KeyEvent {
		public:
			KeyReleasedEvent(int keyCode)
				: KeyEvent(keyCode) {
			}
			std::string ToString() const override {
				std::stringstream ss;
				ss << "KeyReleasedEvent: " << m_KeyCode;
				return ss.str();
			}
			EVENT_CLASS_TYPE(KeyReleased)
	};

	class LUNEX_API KeyTypedEvent : public KeyEvent {
		public:
			KeyTypedEvent(int keycode)
				: KeyEvent(keycode) {
			}

			std::string ToString() const override
			{
				std::stringstream ss;
				ss << "KeyTypedEvent: " << m_KeyCode;
				return ss.str();
			}

			EVENT_CLASS_TYPE(KeyTyped)
	};
} 