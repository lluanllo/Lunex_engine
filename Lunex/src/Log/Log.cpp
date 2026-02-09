#include "stpch.h"
#include "Log.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace Lunex {	
	Ref<spdlog::logger> Log::s_CoreLogger;
	Ref<spdlog::logger> Log::s_ClientLogger;
	std::shared_ptr<CallbackSinkMT> Log::s_CallbackSink;
	
	void Log::Init() {
		std::vector<spdlog::sink_ptr> logSinks;
		logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Lunex.log", true));
		
		// Create callback sink for ConsolePanel
		s_CallbackSink = std::make_shared<CallbackSinkMT>();
		logSinks.emplace_back(s_CallbackSink);
		
		logSinks[0]->set_pattern("%^[%T] %n: %v%$");
		logSinks[1]->set_pattern("[%T] [%l] %n: %v");
		logSinks[2]->set_pattern("%v");  // Callback sink: just the message
		
		s_CoreLogger = std::make_shared<spdlog::logger>("LUNEX", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_CoreLogger);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::trace);
		
		s_ClientLogger = std::make_shared<spdlog::logger>("APP", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::trace);
	}
	
	void Log::Shutdown() {
		ClearLogCallback();
		s_ClientLogger.reset();
		s_CoreLogger.reset();
		s_CallbackSink.reset();
		spdlog::drop_all();
		spdlog::shutdown();
	}
	
	void Log::SetLogCallback(LogCallback callback) {
		if (s_CallbackSink) {
			s_CallbackSink->SetCallback(callback);
		}
	}
	
	void Log::ClearLogCallback() {
		if (s_CallbackSink) {
			s_CallbackSink->SetCallback(nullptr);
		}
	}
}