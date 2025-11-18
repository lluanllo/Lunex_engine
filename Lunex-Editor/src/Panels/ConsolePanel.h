#pragma once

#include "Lunex.h"

#include "imgui.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Lunex {
	enum class LogLevel {
		Trace = 0,
		Info,
		Warning,
		Error,
		Critical
	};
	
	struct ConsoleMessage {
		std::string Message;
		LogLevel Level;
		float Timestamp;
		std::string Category;
		
		ConsoleMessage(const std::string& msg, LogLevel level, const std::string& category = "General")
			: Message(msg), Level(level), Category(category), Timestamp(0.0f) {
			// Get timestamp from ImGui
		}
	};
	
	class ConsoleTab {
		public:
			ConsoleTab(const std::string& name);
			~ConsoleTab() = default;
			
			void AddLog(const std::string& message, LogLevel level, const std::string& category = "General");
			void Clear();
			void Draw();
			
			const std::string& GetName() const { return m_Name; }
			void SetName(const std::string& name) { m_Name = name; }
			
			bool IsActive() const { return m_IsActive; }
			void SetActive(bool active) { m_IsActive = active; }
			
		private:
			std::string m_Name;
			std::vector<ConsoleMessage> m_Messages;
			bool m_AutoScroll = true;
			bool m_ScrollToBottom = false;
			bool m_IsActive = false;
			
			// Filters
			bool m_ShowTrace = true;
			bool m_ShowInfo = true;
			bool m_ShowWarning = true;
			bool m_ShowError = true;
			bool m_ShowCritical = true;
			
			char m_SearchFilter[256] = "";
			std::string m_CategoryFilter = "All";
			std::vector<std::string> m_Categories;
			
			void DrawLogMessage(const ConsoleMessage& msg, int index);
			bool PassesFilters(const ConsoleMessage& msg) const;
			ImVec4 GetLogLevelColor(LogLevel level) const;
			const char* GetLogLevelIcon(LogLevel level) const;
	};
	
	// Command callback function type
	using CommandCallback = std::function<void(const std::vector<std::string>&)>;
	
	struct Command {
		std::string Name;
		std::string Description;
		std::string Usage;
		CommandCallback Callback;
	};
	
	class ConsolePanel {
		public:
			ConsolePanel();
			~ConsolePanel() = default;
			
			void OnImGuiRender();
			
			// Tab management
			void AddTab(const std::string& name);
			void RemoveTab(const std::string& name);
			void SwitchToTab(const std::string& name);
			ConsoleTab* GetActiveTab();
			
			// Logging
			void AddLog(const std::string& message, LogLevel level = LogLevel::Info, const std::string& category = "General");
			void Clear();
			
			// Command system
			void RegisterCommand(const std::string& name, const std::string& description, const std::string& usage, CommandCallback callback);
			void ExecuteCommand(const std::string& commandLine);
			
			// Static instance for global access
			static ConsolePanel& Get();
			
		private:
			std::vector<std::shared_ptr<ConsoleTab>> m_Tabs;
			int m_ActiveTabIndex = 0;
			
			// Command input
			char m_InputBuffer[512] = "";
			std::vector<std::string> m_CommandHistory;
			int m_HistoryPos = -1;
			std::vector<std::string> m_AutoCompleteOptions;
			
			// Registered commands
			std::unordered_map<std::string, Command> m_Commands;
			
			// UI state
			bool m_ReclaimFocus = false;
			bool m_ShowCommandHelp = false;
			
			void DrawTabBar();
			void DrawToolbar();
			void DrawCommandInput();
			void DrawCommandHelp();
			
			void ProcessInput();
			void AutoComplete();
			std::vector<std::string> ParseCommandLine(const std::string& commandLine);
			
			// Built-in commands
			void RegisterBuiltInCommands();
			void CmdHelp(const std::vector<std::string>& args);
			void CmdClear(const std::vector<std::string>& args);
			void CmdEcho(const std::vector<std::string>& args);
			void CmdHistory(const std::vector<std::string>& args);
			void CmdScript(const std::vector<std::string>& args);
			void CmdExit(const std::vector<std::string>& args);
			
			static ConsolePanel* s_Instance;
	};
	
	// Global logging macros for easy access
	#define CONSOLE_TRACE(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Trace)
	#define CONSOLE_INFO(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Info)
	#define CONSOLE_WARN(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Warning)
	#define CONSOLE_ERROR(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Error)
	#define CONSOLE_CRITICAL(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Critical)
}