/**
 * @file LogOutput.h
 * @brief Console log output component for displaying messages
 * 
 * Used by ConsolePanel to render log messages with filtering,
 * colors, and scrolling behavior.
 */

#pragma once

#include "../UICore.h"
#include <vector>
#include <string>
#include <mutex>

namespace Lunex::UI {

	// ============================================================================
	// LOG LEVEL ENUM
	// ============================================================================
	
	enum class LogLevel {
		Trace = 0,
		Info,
		Warning,
		Error,
		Critical,
		
		// Script-specific levels
		ScriptInfo,
		ScriptWarning,
		ScriptError,
		
		// Compilation levels
		CompileStart,
		CompileSuccess,
		CompileError,
		CompileWarning
	};
	
	// ============================================================================
	// LOG MESSAGE STRUCTURE
	// ============================================================================
	
	struct LogMessage {
		std::string Message;
		std::string Category;
		LogLevel Level;
		float Timestamp;
		
		LogMessage() = default;
		LogMessage(const std::string& msg, LogLevel level, const std::string& category = "General")
			: Message(msg), Level(level), Category(category), Timestamp(0.0f) {}
	};
	
	// ============================================================================
	// LOG OUTPUT STYLE
	// ============================================================================
	
	struct LogOutputStyle {
		// Background colors
		Color Background = Color(0.05f, 0.05f, 0.07f, 1.0f);
		Color FilterBarBg = Color(0.12f, 0.12f, 0.12f, 1.0f);
		
		// Text colors by level
		Color TraceColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
		Color InfoColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
		Color WarningColor = Color(1.0f, 0.8f, 0.2f, 1.0f);
		Color ErrorColor = Color(1.0f, 0.3f, 0.3f, 1.0f);
		Color CriticalColor = Color(1.0f, 0.0f, 0.0f, 1.0f);
		
		// Script colors
		Color ScriptInfoColor = Color(0.4f, 0.8f, 1.0f, 1.0f);
		Color ScriptWarningColor = Color(1.0f, 0.7f, 0.3f, 1.0f);
		Color ScriptErrorColor = Color(1.0f, 0.4f, 0.4f, 1.0f);
		
		// Compilation colors
		Color CompileStartColor = Color(0.5f, 0.7f, 1.0f, 1.0f);
		Color CompileSuccessColor = Color(0.3f, 0.9f, 0.3f, 1.0f);
		Color CompileErrorColor = Color(1.0f, 0.2f, 0.2f, 1.0f);
		Color CompileWarningColor = Color(1.0f, 0.6f, 0.2f, 1.0f);
		
		// UI colors
		Color TimestampColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
		Color CategoryColor = Color(0.6f, 0.8f, 1.0f, 1.0f);
		
		// Sizing
		float MessageSpacing = 2.0f;
		float FilterBarHeight = 35.0f;
	};
	
	// ============================================================================
	// LOG OUTPUT FILTERS
	// ============================================================================
	
	struct LogOutputFilters {
		bool ShowTrace = true;
		bool ShowInfo = true;
		bool ShowWarning = true;
		bool ShowError = true;
		bool ShowCritical = true;
		bool ShowScriptMessages = true;
		bool ShowCompileMessages = true;
		
		char SearchFilter[256] = "";
		std::string CategoryFilter = "All";
		
		bool AutoScroll = true;
	};
	
	// ============================================================================
	// LOG OUTPUT COMPONENT
	// ============================================================================
	
	class LogOutput {
	public:
		LogOutput();
		~LogOutput() = default;
		
		// Add messages (thread-safe)
		void AddMessage(const LogMessage& message);
		void AddMessage(const std::string& message, LogLevel level, const std::string& category = "General");
		
		// Clear all messages
		void Clear();
		
		// Rendering
		void RenderFilterBar();
		void RenderMessages();
		void Render();  // Combined filter bar + messages
		
		// Access
		LogOutputStyle& GetStyle() { return m_Style; }
		const LogOutputStyle& GetStyle() const { return m_Style; }
		
		LogOutputFilters& GetFilters() { return m_Filters; }
		const LogOutputFilters& GetFilters() const { return m_Filters; }
		
		size_t GetMessageCount() const { return m_Messages.size(); }
		
		// Categories management
		void AddCategory(const std::string& category);
		const std::vector<std::string>& GetCategories() const { return m_Categories; }
		
	private:
		void DrawLogMessage(const LogMessage& msg, int index);
		bool PassesFilters(const LogMessage& msg) const;
		Color GetLogLevelColor(LogLevel level) const;
		const char* GetLogLevelIcon(LogLevel level) const;
		const char* GetLogLevelName(LogLevel level) const;
		
	private:
		std::vector<LogMessage> m_Messages;
		std::vector<std::string> m_Categories;
		
		LogOutputStyle m_Style;
		LogOutputFilters m_Filters;
		
		bool m_ScrollToBottom = false;
		mutable std::mutex m_Mutex;
		
		static constexpr size_t MAX_MESSAGES = 10000;
	};
	
	// ============================================================================
	// HELPER FUNCTIONS FOR LOG LEVEL
	// ============================================================================
	
	inline bool IsScriptLevel(LogLevel level) {
		return level == LogLevel::ScriptInfo || 
			   level == LogLevel::ScriptWarning || 
			   level == LogLevel::ScriptError;
	}
	
	inline bool IsCompileLevel(LogLevel level) {
		return level == LogLevel::CompileStart || 
			   level == LogLevel::CompileSuccess || 
			   level == LogLevel::CompileError ||
			   level == LogLevel::CompileWarning;
	}

} // namespace Lunex::UI
