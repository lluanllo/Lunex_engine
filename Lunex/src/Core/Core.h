#pragma once

#ifdef LN_PLATFORM_WINDOWS
	#if LN_DYNAMIC_LINK
		#ifdef LN_BUILD_DLL
			#define LUNEX_API __declspec(dllexport)
		#else
			#define LUNEX_API __declspec(dllimport)
		#endif
	#else
		#define LUNEX_API
	#endif
#else
	//#error Lunex only supports Windows!
#endif

#ifdef LN_ENABLE_ASSERTS
	#define LN_ASSERTS(x, ...){ if(!(x)) { LNX_LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define LN_CORE_ASSERT(x, ...){ if(!(x)) { LNX_LOG_ERROR("Core Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define LN_ASSERTS(x, ...)
	#define LN_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define LNX_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)