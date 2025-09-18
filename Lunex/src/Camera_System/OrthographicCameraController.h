#pragma once

#include "Core/Core.h"

#include "Renderer/OrthographicCamera.h"

namespace Lunex {
	class LUNEX_API OrthographicCameraController {
		public:
			OrthographicCameraController();
			
		private:
			OrthographicCamera m_Camera;
	};
}