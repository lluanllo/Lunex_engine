#include "stpch.h"
#include "Core/window.h"

#ifdef LN_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Lunex {
	Scope<Window> Window::Create(const WindowProps& props) {
	#ifdef LN_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
	#else
		LN_CORE_ASSERT(false, "Unknown platform!");
		return nullptr;
	#endif
	}
}