#pragma once

#include "Core/Core.h"
#include "glm/glm.hpp"

namespace Lunex {
	struct DeviceCapabilities {
		int MaxTextureSlots =16;
		int MaxVertexAttributes =16;
	};
	
	class RenderDevice {
		public:
			static void Init();
			static void Shutdown();
			
			static const DeviceCapabilities& GetCapabilities();
			
		private:
			static DeviceCapabilities s_Capabilities;
	};
}
