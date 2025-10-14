#pragma once

#include "Core/Core.h"

#include <string>

namespace Lunex{
	class LUNEX_API FileDialogs {
		public:
			static std::string OpenFile(const char* filter);
			static std::string SaveFile(const char* filter);
	};
}