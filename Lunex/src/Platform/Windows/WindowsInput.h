#pragma once

#include "Core/Input.h"

namespace Lunex {

	class LUNEX_API WindowsInput : public Input {

		protected:
			virtual bool IsKeyPressedImpl(int keycode) override;
		
			virtual bool IsMouseButtonPressedImpl(int button) override;
			virtual std::pair<float, float> GetMousePositionImpl() override;
			virtual float GetMouseXImpl() override;
			virtual float GetMouseYImpl() override;
	};
} // namespace Stellara