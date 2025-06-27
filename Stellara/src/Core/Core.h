#pragma once

#ifdef ST_PLATFORM_WINDOWS
	#if ST_DYNAMIC_LINK
		#ifdef ST_BUILD_DLL
			#define STELLARA_API __declspec(dllexport)
		#else
			#define STELLARA_API __declspec(dllimport)
		#endif
	#else
		#define STELLARA_API
	#endif
#else
	#error Stellara only supports Windows!
#endif

#ifdef ST_ENABLE_ASSERTS
	#define ST_ASSERTS(x, ...){ if(!(x)) { STLR_LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define ST_CORE_ASSERT(x, ...){ if(!(x)) { STLR_LOG_ERROR("Core Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define ST_ASSERTS(x, ...)
	#define ST_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define STLR_BIND_EVENT_FN(fn) std::bind(&ImGuiLayer::fn, this, std::placeholders::_1)
