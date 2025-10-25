#pragma once

#include "Core/Core.h"

#include <string>

namespace Lunex{
	class FileDialogs {
		public:
			static std::string OpenFile(const char* filter);
			static std::string SaveFile(const char* filter);
	};
}