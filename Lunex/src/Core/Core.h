#pragma once

#include <memory>
#include "PlatformDetection.h"

#ifdef LNX_DEBUG
    #if defined(LNX_PLATFORM_WINDOWS)
        #define LNX_DEBUGBREAK() __debugbreak()
    #elif defined(LNX_PLATFORM_LINUX)
        #include <signal.h>
        #define LNX_DEBUGBREAK() raise(SIGTRAP)
    #else
        #error "Platform doesn't support debugbreak yet!"
    #endif
    #define LNX_ENABLE_ASSERTS
#else
    #define LNX_DEBUGBREAK()
#endif

#ifdef LNX_ENABLE_ASSERTS
    #define LNX_ASSERT(x, ...)       { if(!(x)) { LNX_LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); LNX_DEBUGBREAK; } }
    #define LNX_CORE_ASSERT(x, ...)  { if(!(x)) { LNX_LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); LNX_DEBUGBREAK(); } }
#else
    #define LNX_ASSERT(x, ...)
    #define LNX_CORE_ASSERT(x, ...)
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
