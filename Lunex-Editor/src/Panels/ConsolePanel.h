#pragma once

#include "Lunex.h"
#include "../UI/UICore.h"
#include "../UI/UILayout.h"
#include "../UI/UIComponents.h"
#include "../UI/Components/LogOutput.h"

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

	// Re-export LogLevel from UI namespace for backwards compatibility
	using LogLevel = UI::LogLevel;
	// Re-export LogCallbackLevel for internal use
	using LogCallbackLevel = ::Lunex::LogCallbackLevel;
	
	// ============================================================================
	// CONSOLE MESSAGE (Legacy compatibility)
	// ============================================================================
	
	struct ConsoleMessage {
		std::string Message;
		LogLevel Level;
		float Timestamp;
		std::string Category;
		
		ConsoleMessage(const std::string& msg, LogLevel level, const std::string& category = "General")
			: Message(msg), Level(level), Category(category), Timestamp(0.0f) {
		}
	};
	
	// ============================================================================
	// CONSOLE TAB BASE CLASS
	// ============================================================================
	
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
		UI::LogOutput m_LogOutput;
		bool m_IsActive = false;
	};
	
	// ============================================================================
	// TERMINAL TAB - Integrated CMD/PowerShell
	// ============================================================================
	
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
	
	// ============================================================================
	// COMMAND SYSTEM
	// ============================================================================
	
	using CommandCallback = std::function<void(const std::vector<std::string>&)>;
	
	struct ConsoleCommand {
		std::string Name;
		std::string Description;
		std::string Usage;
		CommandCallback Callback;
	};
	
	// ============================================================================
	// CONSOLE PANEL STYLE
	// ============================================================================
	
	struct ConsolePanelStyle {
		UI::Color WindowBg = UI::Color(0.10f, 0.10f, 0.11f, 1.0f);
		UI::Color TabBg = UI::Color(0.16f, 0.16f, 0.16f, 1.0f);
		UI::Color TabActive = UI::Color(0.24f, 0.24f, 0.24f, 1.0f);
		UI::Color TabHovered = UI::Color(0.26f, 0.59f, 0.98f, 0.40f);
		UI::Color InputBg = UI::Color(0.08f, 0.08f, 0.10f, 1.0f);
		UI::Color ToolbarBg = UI::Color(0.12f, 0.12f, 0.13f, 1.0f);
	};
	
	// ============================================================================
	// CONSOLE PANEL
	// ============================================================================
	
	class ConsolePanel {
	public:
		ConsolePanel();
		~ConsolePanel();
		
		void OnImGuiRender();
		
		// Tab management
		void AddTab(const std::string& name);
		void AddTerminalTab(const std::string& name, const std::filesystem::path& workingDirectory, TerminalType type = TerminalType::PowerShell);
		void RemoveTab(const std::string& name);
		void SwitchToTab(const std::string& name);
		ConsoleTab* GetActiveTab();
		
		// Logging - Main API
		void AddLog(const std::string& message, LogLevel level = LogLevel::Info, const std::string& category = "General");
		void Clear();
		
		// Script-specific logging
		void AddScriptLog(const std::string& message, LogLevel level = LogLevel::ScriptInfo);
		
		// Compilation logging
		void AddCompileLog(const std::string& message, bool isError = false, bool isWarning = false);
		void AddCompileStart(const std::string& scriptName);
		void AddCompileSuccess(const std::string& scriptName);
		void AddCompileError(const std::string& scriptName, const std::string& error);
		
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
		
		// Style access
		ConsolePanelStyle& GetStyle() { return m_Style; }
		const ConsolePanelStyle& GetStyle() const { return m_Style; }
		
		// Enable/disable engine log forwarding
		void SetEngineLogForwarding(bool enabled);
		bool IsEngineLogForwardingEnabled() const { return m_EngineLogForwarding; }
		
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
		
		// Style
		ConsolePanelStyle m_Style;
		
		// Engine log forwarding
		bool m_EngineLogForwarding = true;
		
		// Get the working directory for new terminals
		std::filesystem::path GetTerminalWorkingDirectory() const;
		
		void DrawTabBar();
		void DrawToolbar();
		void DrawCommandInput();
		void DrawCommandHelp();
		
		void ProcessInput();
		void AutoComplete();
		std::vector<std::string> ParseCommandLine(const std::string& commandLine);
		
		// Log callback handler
		void OnEngineLog(LogCallbackLevel level, const std::string& message, const std::string& category);
		
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
	
	// ============================================================================
	// GLOBAL LOGGING MACROS
	// ============================================================================
	
	#define CONSOLE_TRACE(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Trace)
	#define CONSOLE_INFO(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Info)
	#define CONSOLE_WARN(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Warning)
	#define CONSOLE_ERROR(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Error)
	#define CONSOLE_CRITICAL(msg, ...) Lunex::ConsolePanel::Get().AddLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::Critical)
	
	// Script-specific macros
	#define CONSOLE_SCRIPT(msg, ...) Lunex::ConsolePanel::Get().AddScriptLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::ScriptInfo)
	#define CONSOLE_SCRIPT_WARN(msg, ...) Lunex::ConsolePanel::Get().AddScriptLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::ScriptWarning)
	#define CONSOLE_SCRIPT_ERROR(msg, ...) Lunex::ConsolePanel::Get().AddScriptLog(fmt::format(msg, __VA_ARGS__), Lunex::LogLevel::ScriptError)
}