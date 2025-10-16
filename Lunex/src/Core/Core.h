#pragma once

#include <memory>
#include "PlatformDetection.h"

#ifdef LN_DEBUG
    #if defined(LN_PLATFORM_WINDOWS)
        #define LN_DEBUGBREAK() __debugbreak()
    #elif defined(LN_PLATFORM_LINUX)
        #include <signal.h>
        #define LN_DEBUGBREAK() raise(SIGTRAP)
    #else
        #error "Platform doesn't support debugbreak yet!"
    #endif
    #define LN_ENABLE_ASSERTS
#else
    #define LN_DEBUGBREAK()
#endif

#ifdef LN_ENABLE_ASSERTS
    #define LN_ASSERT(x, ...)       { if(!(x)) { LNX_LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); LN_DEBUGBREAK; } }
    #define LN_CORE_ASSERT(x, ...)  { if(!(x)) { LNX_LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); LN_DEBUGBREAK(); } }
#else
    #define LN_ASSERT(x, ...)
    #define LN_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define LNX_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Lunex {
     
    template<typename T>
    using Scope = std::unique_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Scope<T> CreateScope(Args&& ... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
     
    template<typename T>
    using Ref = std::shared_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Ref<T> CreateRef(Args&& ... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
}
