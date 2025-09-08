#pragma once

#include "Core/Core.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace Lunex {

	class LUNEX_API Log {
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};

#define LNX_LOG_TRACE(...)    ::Lunex::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LNX_LOG_INFO(...)     ::Lunex::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LNX_LOG_WARN(...)     ::Lunex::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LNX_LOG_ERROR(...)    ::Lunex::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LNX_LOG_CRITICAL(...) ::Lunex::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define LNX_LOG_FATAL(...)    ::Lunex::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define LNX_LOG_DEBUG(...)    ::Lunex::Log::GetCoreLogger()->debug(__VA_ARGS__)

}