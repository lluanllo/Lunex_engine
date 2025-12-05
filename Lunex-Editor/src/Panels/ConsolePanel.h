#pragma once

#include "Lunex.h"

#include "imgui.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#endif

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
		}
	};
	
	class ConsoleTab {
		public:
			ConsoleTab(const std::string& name);
			virtual ~ConsoleTab() = default;
			
			virtual void AddLog(const std::string& message, LogLevel level, const std::string& category = "General");
			virtual void Clear();
			virtual void Draw();
			
			const std::string& GetName() const { return m_Name; }
			void SetName(const std::string& name) { m_Name = name; }
			
			bool IsActive() const { return m_IsActive; }
			void SetActive(bool active) { m_IsActive = active; }
			
			virtual bool IsTerminal() const { return false; }
			
		protected:
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
	
	// ========================================
	// TERMINAL TAB - Integrated CMD/PowerShell
	// ========================================
	enum class TerminalType {
		CMD,
		PowerShell
	};
	
	class TerminalTab : public ConsoleTab {
	public:
		TerminalTab(const std::string& name, const std::filesystem::path& workingDirectory, TerminalType type = TerminalType::PowerShell);
		~TerminalTab() override;
		
		void Draw() override;
		void Clear() override;
		
		bool IsTerminal() const override { return true; }
		bool IsRunning() const { return m_IsRunning; }
		
		void SendCommand(const std::string& command);
		void Terminate();
		
		const std::filesystem::path& GetWorkingDirectory() const { return m_WorkingDirectory; }
		TerminalType GetTerminalType() const { return m_TerminalType; }
		
	private:
		std::filesystem::path m_WorkingDirectory;
		TerminalType m_TerminalType;
		
		// Process management (Windows)
#ifdef _WIN32
		HANDLE m_ProcessHandle = NULL;
		HANDLE m_ThreadHandle = NULL;
		HANDLE m_StdInRead = NULL;
		HANDLE m_StdInWrite = NULL;
		HANDLE m_StdOutRead = NULL;
		HANDLE m_StdOutWrite = NULL;
#endif
		
		// Output buffer
		std::vector<std::string> m_OutputLines;
		std::mutex m_OutputMutex;
		std::atomic<bool> m_IsRunning{false};
		std::atomic<bool> m_ShouldStop{false};
		
		// Reader thread
		std::thread m_ReaderThread;
		
		// Input
		char m_InputBuffer[1024] = "";
		std::vector<std::string> m_CommandHistory;
		int m_HistoryPos = -1;
		bool m_ReclaimFocus = false;
		bool m_ScrollToBottom = false;
		
		// Internal methods
		bool StartProcess();
		void StopProcess();
		void ReadOutputThread();
		void AppendOutput(const std::string& text);
		
		void DrawTerminalOutput();
		void DrawTerminalInput();
		void ProcessTerminalInput();
	};
	
	// Command callback function type
	using CommandCallback = std::function<void(const std::vector<std::string>&)>;
	
	struct ConsoleCommand {
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
			void AddTerminalTab(const std::string& name, const std::filesystem::path& workingDirectory, TerminalType type = TerminalType::PowerShell);
			void RemoveTab(const std::string& name);
			void SwitchToTab(const std::string& name);
			ConsoleTab* GetActiveTab();
			
			// Logging
			void AddLog(const std::string& message, LogLevel level = LogLevel::Info, const std::string& category = "General");
			void Clear();
			
			// Command system
			void RegisterCommand(const std::string& name, const std::string& description, const std::string& usage, CommandCallback callback);
			void ExecuteCommand(const std::string& commandLine);
			
			// Visibility
			void Toggle() { m_IsOpen = !m_IsOpen; }
			bool IsOpen() const { return m_IsOpen; }
			void SetOpen(bool open) { m_IsOpen = open; }
			
			// Project directory for terminal (fallback if no active project)
			void SetProjectDirectory(const std::filesystem::path& path) { m_ProjectDirectory = path; }
			const std::filesystem::path& GetProjectDirectory() const { return m_ProjectDirectory; }
			
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
			std::unordered_map<std::string, ConsoleCommand> m_Commands;
			
			// UI state
			bool m_ReclaimFocus = false;
			bool m_ShowCommandHelp = false;
			bool m_IsOpen = true;
			
			// Project directory (fallback)
			std::filesystem::path m_ProjectDirectory;
			
			// Get the working directory for new terminals
			std::filesystem::path GetTerminalWorkingDirectory() const;
			
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