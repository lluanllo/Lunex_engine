#pragma once

#ifdef ST_ENABLE_ASSERTS
	#define ST_ASSERTS(x, ...){ if(!(x)) { STLR_LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define ST_CORE_ASSERT(x, ...){ if(!(x)) { STLR_LOG_ERROR("Core Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define ST_ASSERTS(x, ...)
	#define ST_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)