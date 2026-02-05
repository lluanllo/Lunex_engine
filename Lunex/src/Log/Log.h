#pragma once

#include "Core/Core.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"

// This ignores all warnings raised inside External headers
#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/base_sink.h>
#pragma warning(pop)

#include <functional>
#include <mutex>

namespace Lunex {

	// ============================================================================
	// LOG CALLBACK SYSTEM
	// ============================================================================
	
	enum class LogCallbackLevel {
		Trace = 0,
		Debug,
		Info,
		Warn,
		Error,
		Critical
	};
	
	using LogCallback = std::function<void(LogCallbackLevel level, const std::string& message, const std::string& category)>;
	
	// ============================================================================
	// CUSTOM SINK FOR CONSOLE PANEL
	// ============================================================================
	
	template<typename Mutex>
	class CallbackSink : public spdlog::sinks::base_sink<Mutex> {
	public:
		void SetCallback(LogCallback callback) {
			m_Callback = callback;
		}
		
	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override {
			if (!m_Callback) return;
			
			spdlog::memory_buf_t formatted;
			spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
			std::string message = fmt::to_string(formatted);
			
			// Remove trailing newline if present
			if (!message.empty() && message.back() == '\n') {
				message.pop_back();
			}
			
			// Convert spdlog level to our level
			LogCallbackLevel level;
			switch (msg.level) {
				case spdlog::level::trace:    level = LogCallbackLevel::Trace; break;
				case spdlog::level::debug:    level = LogCallbackLevel::Debug; break;
				case spdlog::level::info:     level = LogCallbackLevel::Info; break;
				case spdlog::level::warn:     level = LogCallbackLevel::Warn; break;
				case spdlog::level::err:      level = LogCallbackLevel::Error; break;
				case spdlog::level::critical: level = LogCallbackLevel::Critical; break;
				default:                      level = LogCallbackLevel::Info; break;
			}
			
			// Determine category from logger name
			std::string category = "Engine";
			std::string loggerName = std::string(msg.logger_name.data(), msg.logger_name.size());
			
			// Check for special prefixes in the message
			if (message.find("[Script]") != std::string::npos) {
				category = "Script";
			}
			else if (message.find("[Compiler]") != std::string::npos) {
				category = "Compiler";
			}
			else if (loggerName == "APP") {
				category = "Application";
			}
			
			m_Callback(level, message, category);
		}
		
		void flush_() override {}
		
	private:
		LogCallback m_Callback;
	};
	
	using CallbackSinkMT = CallbackSink<std::mutex>;
	
	// ============================================================================
	// LOG CLASS
	// ============================================================================
	
	class Log {
	public:
		static void Init();
		
		static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
		
		// Set callback to receive log messages (for ConsolePanel integration)
		static void SetLogCallback(LogCallback callback);
		static void ClearLogCallback();
		
	private:
		static Ref<spdlog::logger> s_CoreLogger;
		static Ref<spdlog::logger> s_ClientLogger;
		static std::shared_ptr<CallbackSinkMT> s_CallbackSink;
	};
	
	template<typename OStream, glm::length_t L, typename T, glm::qualifier Q>
	inline OStream& operator<<(OStream& os, const glm::vec<L, T, Q>& vector) {
		return os << glm::to_string(vector);
	}
	
	template<typename OStream, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
	inline OStream& operator<<(OStream& os, const glm::mat<C, R, T, Q>& matrix) {
		return os << glm::to_string(matrix);
	}
	
	template<typename OStream, typename T, glm::qualifier Q>
	inline OStream& operator<<(OStream& os, glm::qua<T, Q> quaternion) {
		return os << glm::to_string(quaternion);
	}
	
	#define LNX_LOG_TRACE(...)    ::Lunex::Log::GetCoreLogger()->trace(__VA_ARGS__)
	#define LNX_LOG_INFO(...)     ::Lunex::Log::GetCoreLogger()->info(__VA_ARGS__)
	#define LNX_LOG_WARN(...)     ::Lunex::Log::GetCoreLogger()->warn(__VA_ARGS__)
	#define LNX_LOG_ERROR(...)    ::Lunex::Log::GetCoreLogger()->error(__VA_ARGS__)
	#define LNX_LOG_CRITICAL(...) ::Lunex::Log::GetCoreLogger()->critical(__VA_ARGS__)
	#define LNX_LOG_DEBUG(...)    ::Lunex::Log::GetCoreLogger()->debug(__VA_ARGS__)
}