#pragma once

#include "Input.h"

namespace Stellara {

	class WindowsInput : public Input {

		protected:
			virtual bool IsKeyPressedImpl(int keycode) override;
		
			virtual bool IsMouseButtonPressedImpl(int button) override;
			virtual std::pair<float, float> GetMousePositionImpl() override;
			virtual float GetMouseXImpl() override;
			virtual float GetMouseYImpl() override;
	};
} // namespace Stellara