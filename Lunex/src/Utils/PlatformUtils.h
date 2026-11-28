#pragma once

#include "Core/Core.h"

#include <string>
#include <vector>

namespace Lunex{
	class FileDialogs {
		public:
			static std::string OpenFile(const char* filter);
			// Multi-selection open dialog; returns full paths (empty when cancelled)
			static std::vector<std::string> OpenFiles(const char* filter, const char* title = nullptr);
			static std::string SaveFile(const char* filter);
			static std::string SelectFolder();
	};

	// Starts a new instance of the running executable with `args`; the caller then closes this one
	bool RestartApplication(const std::vector<std::string>& args);

	// Position / size / maximized state of the main window (kept across a restart)
	struct WindowPlacement { int X = 0, Y = 0, Width = 0, Height = 0; bool Maximized = false; };
	WindowPlacement GetMainWindowPlacement();
	void SetMainWindowPlacement(const WindowPlacement& placement);
}