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
}