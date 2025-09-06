#pragma once

#include "Core/Core.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/ostr.h"

namespace Lunex {

	class LUNEX_API Log {
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

	private:
		static std::shared_ptr<spdlog::logger> s_Logger;
	};

#define LNX_LOG_TRACE(...)    ::Lunex::Log::GetLogger()->trace(__VA_ARGS__)
#define LNX_LOG_INFO(...)     ::Lunex::Log::GetLogger()->info(__VA_ARGS__)
#define LNX_LOG_WARN(...)     ::Lunex::Log::GetLogger()->warn(__VA_ARGS__)
#define LNX_LOG_ERROR(...)    ::Lunex::Log::GetLogger()->error(__VA_ARGS__)
#define LNX_LOG_CRITICAL(...) ::Lunex::Log::GetLogger()->critical(__VA_ARGS__)
#define LNX_LOG_FATAL(...)    ::Lunex::Log::GetLogger()->critical(__VA_ARGS__)
#define LNX_LOG_DEBUG(...)    ::Lunex::Log::GetLogger()->debug(__VA_ARGS__)

}