#pragma once

#include "Core/Core.h"
#include "Core/Input.h"
#include "Event.h"

namespace Lunex {
		class LUNEX_API KeyEvent : public Event {
			public:
				KeyCode GetKeyCode() const { return m_KeyCode; }
				
				EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
			protected:
				KeyEvent(KeyCode keyCode)
					: m_KeyCode(keyCode) {
				}
				KeyCode m_KeyCode;
	};
	
	class LUNEX_API KeyPressedEvent : public KeyEvent {
		public:
			KeyPressedEvent(KeyCode keyCode, int repeatCount)
				: KeyEvent(keyCode), m_RepeatCount(repeatCount) {
			}
			
			int GetRepeatCount() const { return m_RepeatCount; }
			
			std::string ToString() const override {
				std::stringstream ss;
				ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
				return ss.str();
			}
			
			EVENT_CLASS_TYPE(KeyPressed)
		private:
			int m_RepeatCount;
	};
	
	class LUNEX_API KeyReleasedEvent : public KeyEvent {
		public:
			KeyReleasedEvent(KeyCode keyCode)
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
			KeyTypedEvent(KeyCode keycode)
				: KeyEvent(keycode) {
			}
			
			std::string ToString() const override {
				std::stringstream ss;
				ss << "KeyTypedEvent: " << m_KeyCode;
				return ss.str();
			}
			
			EVENT_CLASS_TYPE(KeyTyped)
	};
} 