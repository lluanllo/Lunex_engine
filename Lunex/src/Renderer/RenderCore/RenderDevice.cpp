#include "stpch.h"
#include "RenderDevice.h"

namespace Lunex {
	DeviceCapabilities RenderDevice::s_Capabilities;
	
	void RenderDevice::Init() {
		LNX_PROFILE_FUNCTION();
		// Query real capabilities from the backend later. For now set sensible defaults.
		s_Capabilities.MaxTextureSlots =32;
		s_Capabilities.MaxVertexAttributes =16;
	}
	
	void RenderDevice::Shutdown() {
		// nothing for now
	}
	
	const DeviceCapabilities& RenderDevice::GetCapabilities() {
		return s_Capabilities;
	}
}
