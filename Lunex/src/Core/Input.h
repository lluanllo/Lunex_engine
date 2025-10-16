#pragma once

#include "Core/Core.h"
#include "KeyCodes.h"
#include "MouseButtonCodes.h"

namespace Lunex {
	class   Input {		
		public:
			static bool IsKeyPressed(KeyCode key);
			
			static bool IsMouseButtonPressed(MouseCode button);
			static std::pair<float, float> GetMousePosition();
			static float GetMouseX();
			static float GetMouseY();
	};
}