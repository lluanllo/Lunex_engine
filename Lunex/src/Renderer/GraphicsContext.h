#pragma once

namespace Lunex {

	class LUNEX_API GraphicsContext {
		public:
			virtual void Init() = 0;
			virtual void SwapBuffers() = 0;
	};
}