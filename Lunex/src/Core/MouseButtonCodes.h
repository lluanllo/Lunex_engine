#pragma once

namespace Lunex {
	typedef enum class MouseCode : uint16_t {
		// From glfw3.h
		Button0 = 0,
		Button1 = 1,
		Button2 = 2,
		Button3 = 3,
		Button4 = 4,
		Button5 = 5,
		Button6 = 6,
		Button7 = 7,
		
		ButtonLast = Button7,
		ButtonLeft = Button0,
		ButtonRight = Button1,
		ButtonMiddle = Button2
	} Mouse;
	
	inline std::ostream& operator<<(std::ostream& os, MouseCode mouseCode) {
		os << static_cast<int32_t>(mouseCode);
		return os;
	}
}

//from glfw3.h

#define LN_MOUSE_BUTTON_1         0
#define LN_MOUSE_BUTTON_2         1
#define LN_MOUSE_BUTTON_3         2
#define LN_MOUSE_BUTTON_4         3
#define LN_MOUSE_BUTTON_5         4
#define LN_MOUSE_BUTTON_6         5
#define LN_MOUSE_BUTTON_7         6
#define LN_MOUSE_BUTTON_8         7
#define LN_MOUSE_BUTTON_LAST      LN_MOUSE_BUTTON_8
#define LN_MOUSE_BUTTON_LEFT      LN_MOUSE_BUTTON_1
#define LN_MOUSE_BUTTON_RIGHT     LN_MOUSE_BUTTON_2
#define LN_MOUSE_BUTTON_MIDDLE    LN_MOUSE_BUTTON_3