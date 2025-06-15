#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace Stellara{

	class Log{
		public:
			static void Init();

			inline static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

		private:
			static std::shared_ptr<spdlog::logger> s_Logger;
	};

	#define STLR_LOG_TRACE(...)    ::Stellara::Log::GetLogger()->trace(__VA_ARGS__)
	#define STLR_LOG_INFO(...)     ::Stellara::Log::GetLogger()->info(__VA_ARGS__)
	#define STLR_LOG_WARN(...)     ::Stellara::Log::GetLogger()->warn(__VA_ARGS__)
	#define STLR_LOG_ERROR(...)    ::Stellara::Log::GetLogger()->error(__VA_ARGS__)
	#define STLR_LOG_CRITICAL(...) ::Stellara::Log::GetLogger()->critical(__VA_ARGS__)
	#define STLR_LOG_FATAL(...)    ::Stellara::Log::GetLogger()->critical(__VA_ARGS__)
	#define STLR_LOG_DEBUG(...)    ::Stellara::Log::GetLogger()->debug(__VA_ARGS__)

}

