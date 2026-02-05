#include "ConsolePanel.h"
#include "Project/Project.h"
#include "Log/Log.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <chrono>

namespace Lunex {

	ConsolePanel* ConsolePanel::s_Instance = nullptr;

	// ========== ConsoleTab Implementation ==========

	ConsoleTab::ConsoleTab(const std::string& name)
		: m_Name(name) {
	}

	void ConsoleTab::AddLog(const std::string& message, LogLevel level, const std::string& category) {
		m_LogOutput.AddMessage(message, level, category);
	}

	void ConsoleTab::Clear() {
		m_LogOutput.Clear();
	}

	void ConsoleTab::Draw() {
		m_LogOutput.Render();
	}

	// ========================================
	// TERMINAL TAB IMPLEMENTATION
	// ========================================

	TerminalTab::TerminalTab(const std::string& name, const std::filesystem::path& workingDirectory, TerminalType type)
		: ConsoleTab(name), m_WorkingDirectory(workingDirectory), m_TerminalType(type) {

		// Start the terminal process
		if (!StartProcess()) {
			AppendOutput("[ERROR] Failed to start terminal process\n");
		}
	}

	TerminalTab::~TerminalTab() {
		Terminate();
	}

	bool TerminalTab::StartProcess() {
#ifdef _WIN32
		SECURITY_ATTRIBUTES saAttr;
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		// Create pipes for stdin
		if (!CreatePipe(&m_StdInRead, &m_StdInWrite, &saAttr, 0)) {
			return false;
		}
		SetHandleInformation(m_StdInWrite, HANDLE_FLAG_INHERIT, 0);

		// Create pipes for stdout
		if (!CreatePipe(&m_StdOutRead, &m_StdOutWrite, &saAttr, 0)) {
			CloseHandle(m_StdInRead);
			CloseHandle(m_StdInWrite);
			return false;
		}
		SetHandleInformation(m_StdOutRead, HANDLE_FLAG_INHERIT, 0);

		// Setup startup info
		STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.hStdError = m_StdOutWrite;
		si.hStdOutput = m_StdOutWrite;
		si.hStdInput = m_StdInRead;
		si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));

		// Build command line
		std::wstring cmdLine;
		if (m_TerminalType == TerminalType::PowerShell) {
			cmdLine = L"powershell.exe -NoLogo -NoExit -Command \"$Host.UI.RawUI.WindowTitle = 'Lunex Terminal'\"";
		}
		else {
			cmdLine = L"cmd.exe /K \"title Lunex Terminal\"";
		}

		// Convert working directory to wide string
		std::wstring workingDirW = m_WorkingDirectory.wstring();

		// Create process
		if (!CreateProcessW(
			NULL,
			cmdLine.data(),
			NULL,
			NULL,
			TRUE,
			CREATE_NO_WINDOW,
			NULL,
			workingDirW.empty() ? NULL : workingDirW.c_str(),
			&si,
			&pi
		)) {
			CloseHandle(m_StdInRead);
			CloseHandle(m_StdInWrite);
			CloseHandle(m_StdOutRead);
			CloseHandle(m_StdOutWrite);
			return false;
		}

		m_ProcessHandle = pi.hProcess;
		m_ThreadHandle = pi.hThread;
		m_IsRunning = true;
		m_ShouldStop = false;

		// Start reader thread
		m_ReaderThread = std::thread(&TerminalTab::ReadOutputThread, this);

		// Show startup message
		std::string typeStr = (m_TerminalType == TerminalType::PowerShell) ? "PowerShell" : "CMD";
		AppendOutput("[Lunex Terminal] Started " + typeStr + " in: " + m_WorkingDirectory.string() + "\n");
		AppendOutput("[Lunex Terminal] Type 'exit' to close the terminal\n\n");

		return true;
#else
		AppendOutput("[ERROR] Terminal not supported on this platform\n");
		return false;
#endif
	}

	void TerminalTab::StopProcess() {
#ifdef _WIN32
		m_ShouldStop = true;
		m_IsRunning = false;

		// Close write handle to signal EOF to the process
		if (m_StdInWrite) {
			CloseHandle(m_StdInWrite);
			m_StdInWrite = NULL;
		}

		// Wait for reader thread to finish
		if (m_ReaderThread.joinable()) {
			m_ReaderThread.join();
		}

		// Terminate process if still running
		if (m_ProcessHandle) {
			TerminateProcess(m_ProcessHandle, 0);
			WaitForSingleObject(m_ProcessHandle, 1000);
			CloseHandle(m_ProcessHandle);
			m_ProcessHandle = NULL;
		}

		if (m_ThreadHandle) {
			CloseHandle(m_ThreadHandle);
			m_ThreadHandle = NULL;
		}

		if (m_StdInRead) {
			CloseHandle(m_StdInRead);
			m_StdInRead = NULL;
		}

		if (m_StdOutRead) {
			CloseHandle(m_StdOutRead);
			m_StdOutRead = NULL;
		}

		if (m_StdOutWrite) {
			CloseHandle(m_StdOutWrite);
			m_StdOutWrite = NULL;
		}
#endif
	}

	void TerminalTab::ReadOutputThread() {
#ifdef _WIN32
		char buffer[4096];
		DWORD bytesRead;

		while (!m_ShouldStop && m_IsRunning) {
			// Check if there's data available
			DWORD bytesAvailable = 0;
			if (!PeekNamedPipe(m_StdOutRead, NULL, 0, NULL, &bytesAvailable, NULL)) {
				break;
			}

			if (bytesAvailable > 0) {
				if (ReadFile(m_StdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
					buffer[bytesRead] = '\0';
					AppendOutput(std::string(buffer, bytesRead));
				}
			}
			else {
				// Small sleep to avoid busy-waiting
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			// Check if process is still running
			DWORD exitCode;
			if (GetExitCodeProcess(m_ProcessHandle, &exitCode) && exitCode != STILL_ACTIVE) {
				m_IsRunning = false;
				AppendOutput("\n[Lunex Terminal] Process exited with code " + std::to_string(exitCode) + "\n");
				break;
			}
		}
#endif
	}

	void TerminalTab::AppendOutput(const std::string& text) {
		std::lock_guard<std::mutex> lock(m_OutputMutex);

		// Split text by newlines and append to output lines
		std::string remaining = text;
		size_t pos;

		while ((pos = remaining.find('\n')) != std::string::npos) {
			std::string line = remaining.substr(0, pos);

			// Remove carriage return if present
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}

			if (!m_OutputLines.empty() && !m_OutputLines.back().empty() && m_OutputLines.back().back() != '\n') {
				// Append to last line if it doesn't end with newline
				m_OutputLines.back() += line;
				m_OutputLines.push_back("");
			}
			else {
				m_OutputLines.push_back(line);
			}

			remaining = remaining.substr(pos + 1);
		}

		// Handle remaining text (no newline at end)
		if (!remaining.empty()) {
			// Remove carriage return if present
			if (remaining.back() == '\r') {
				remaining.pop_back();
			}

			if (!m_OutputLines.empty()) {
				m_OutputLines.back() += remaining;
			}
			else {
				m_OutputLines.push_back(remaining);
			}
		}

		// Limit output buffer size
		const size_t maxLines = 10000;
		if (m_OutputLines.size() > maxLines) {
			m_OutputLines.erase(m_OutputLines.begin(), m_OutputLines.begin() + (m_OutputLines.size() - maxLines));
		}

		m_ScrollToBottom = true;
	}

	void TerminalTab::SendCommand(const std::string& command) {
#ifdef _WIN32
		if (!m_IsRunning || !m_StdInWrite) {
			AppendOutput("[ERROR] Terminal not running\n");
			return;
		}

		std::string cmdWithNewline = command + "\r\n";
		DWORD bytesWritten;

		if (!WriteFile(m_StdInWrite, cmdWithNewline.c_str(), (DWORD)cmdWithNewline.size(), &bytesWritten, NULL)) {
			AppendOutput("[ERROR] Failed to send command\n");
		}
		FlushFileBuffers(m_StdInWrite);
#endif
	}

	void TerminalTab::Terminate() {
		StopProcess();
	}

	void TerminalTab::Clear() {
		std::lock_guard<std::mutex> lock(m_OutputMutex);
		m_OutputLines.clear();
	}

	void TerminalTab::Draw() {
		using namespace UI;

		// ===== TERMINAL HEADER =====
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));

		if (ImGui::BeginChild("##terminal_header", ImVec2(0, 30), true, ImGuiWindowFlags_NoScrollbar)) {
			// Terminal type indicator
			const char* typeIcon = (m_TerminalType == TerminalType::PowerShell) ? "PS>" : "CMD>";
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
			ImGui::Text("%s", typeIcon);
			ImGui::PopStyleColor();

			ImGui::SameLine();

			// Working directory
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
			ImGui::Text("%s", m_WorkingDirectory.string().c_str());
			ImGui::PopStyleColor();

			ImGui::SameLine(ImGui::GetWindowWidth() - 150);

			// Status indicator
			if (m_IsRunning) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
				ImGui::Text("Running");
				ImGui::PopStyleColor();
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
				ImGui::Text("Stopped");
				ImGui::PopStyleColor();
			}

			ImGui::SameLine();

			if (ImGui::Button("Clear")) {
				Clear();
			}
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		// ===== TERMINAL OUTPUT =====
		DrawTerminalOutput();

		// ===== TERMINAL INPUT =====
		DrawTerminalInput();
	}

	void TerminalTab::DrawTerminalOutput() {
		// Terminal-style dark background
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.07f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		float inputHeight = 30.0f;
		if (ImGui::BeginChild("##terminal_output", ImVec2(0, -inputHeight), false, ImGuiWindowFlags_HorizontalScrollbar)) {
			// Use monospace-style rendering
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

			{
				std::lock_guard<std::mutex> lock(m_OutputMutex);
				for (const auto& line : m_OutputLines) {
					ImGui::TextUnformatted(line.c_str());
				}
			}

			ImGui::PopStyleColor();

			// Auto-scroll
			if (m_ScrollToBottom) {
				ImGui::SetScrollHereY(1.0f);
				m_ScrollToBottom = false;
			}
		}
		ImGui::EndChild();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	void TerminalTab::DrawTerminalInput() {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));

		ImGui::PushItemWidth(-1);

		// Input prompt
		const char* prompt = (m_TerminalType == TerminalType::PowerShell) ? "PS> " : "> ";

		ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackHistory |
			ImGuiInputTextFlags_CallbackCompletion;

		// History callback
		auto historyCallback = [](ImGuiInputTextCallbackData* data) -> int {
			TerminalTab* terminal = (TerminalTab*)data->UserData;

			if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
				int prevHistoryPos = terminal->m_HistoryPos;

				if (data->EventKey == ImGuiKey_UpArrow) {
					if (terminal->m_HistoryPos == -1)
						terminal->m_HistoryPos = (int)terminal->m_CommandHistory.size() - 1;
					else if (terminal->m_HistoryPos > 0)
						terminal->m_HistoryPos--;
				}
				else if (data->EventKey == ImGuiKey_DownArrow) {
					if (terminal->m_HistoryPos != -1) {
						terminal->m_HistoryPos++;
						if (terminal->m_HistoryPos >= (int)terminal->m_CommandHistory.size())
							terminal->m_HistoryPos = -1;
					}
				}

				if (prevHistoryPos != terminal->m_HistoryPos) {
					const char* historyStr = (terminal->m_HistoryPos >= 0) ?
						terminal->m_CommandHistory[terminal->m_HistoryPos].c_str() : "";
					data->DeleteChars(0, data->BufTextLen);
					data->InsertChars(0, historyStr);
				}
			}

			return 0;
			};

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
		ImGui::Text("%s", prompt);
		ImGui::PopStyleColor();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
		if (ImGui::InputText("##terminal_input", m_InputBuffer, sizeof(m_InputBuffer), inputFlags, historyCallback, this)) {
			ProcessTerminalInput();
			m_ReclaimFocus = true;
		}
		ImGui::PopStyleColor();

		// Keep focus on input
		if (m_ReclaimFocus) {
			ImGui::SetKeyboardFocusHere(-1);
			m_ReclaimFocus = false;
		}

		ImGui::PopItemWidth();
		ImGui::PopStyleColor(3);
	}

	void TerminalTab::ProcessTerminalInput() {
		std::string command = m_InputBuffer;

		if (command.empty())
			return;

		// Add to history
		m_CommandHistory.push_back(command);
		m_HistoryPos = -1;

		// Clear input
		memset(m_InputBuffer, 0, sizeof(m_InputBuffer));

		// Send to process
		SendCommand(command);
	}

	// ========== ConsolePanel Implementation ==========

	ConsolePanel::ConsolePanel() {
		s_Instance = this;

		// Create default tab
		AddTab("Main");

		// Register built-in commands
		RegisterBuiltInCommands();

		// Connect to engine logging system
		Log::SetLogCallback([this](LogCallbackLevel level, const std::string& message, const std::string& category) {
			OnEngineLog(level, message, category);
			});

		AddLog("Console initialized. Type 'help' for available commands.", LogLevel::Info, "System");
	}

	ConsolePanel::~ConsolePanel() {
		// Disconnect from engine logging system
		Log::ClearLogCallback();

		if (s_Instance == this) {
			s_Instance = nullptr;
		}
	}

	ConsolePanel& ConsolePanel::Get() {
		if (!s_Instance) {
			static ConsolePanel instance;
			return instance;
		}
		return *s_Instance;
	}

	void ConsolePanel::OnEngineLog(LogCallbackLevel level, const std::string& message, const std::string& category) {
		if (!m_EngineLogForwarding) return;

		// Convert LogCallbackLevel to LogLevel
		LogLevel logLevel;

		// Check for special categories first
		if (category == "Script" || message.find("[Script]") != std::string::npos) {
			if (level == LogCallbackLevel::Error || level == LogCallbackLevel::Critical) {
				logLevel = LogLevel::ScriptError;
			}
			else if (level == LogCallbackLevel::Warn) {
				logLevel = LogLevel::ScriptWarning;
			}
			else {
				logLevel = LogLevel::ScriptInfo;
			}
		}
		else if (category == "Compiler" || message.find("[Compiler]") != std::string::npos) {
			if (message.find("error") != std::string::npos || message.find("Error") != std::string::npos) {
				logLevel = LogLevel::CompileError;
			}
			else if (message.find("warning") != std::string::npos || message.find("Warning") != std::string::npos) {
				logLevel = LogLevel::CompileWarning;
			}
			else if (message.find("Compiling") != std::string::npos || message.find("===") != std::string::npos) {
				logLevel = LogLevel::CompileStart;
			}
			else if (message.find("successfully") != std::string::npos || message.find("compiled") != std::string::npos) {
				logLevel = LogLevel::CompileSuccess;
			}
			else {
				logLevel = LogLevel::Info;
			}
		}
		else {
			// Standard level conversion
			switch (level) {
			case LogCallbackLevel::Trace: logLevel = LogLevel::Trace; break;
			case LogCallbackLevel::Debug: logLevel = LogLevel::Trace; break;
			case LogCallbackLevel::Info:  logLevel = LogLevel::Info; break;
			case LogCallbackLevel::Warn:  logLevel = LogLevel::Warning; break;
			case LogCallbackLevel::Error: logLevel = LogLevel::Error; break;
			case LogCallbackLevel::Critical: logLevel = LogLevel::Critical; break;
			default: logLevel = LogLevel::Info; break;
			}
		}

		AddLog(message, logLevel, category);
	}

	void ConsolePanel::SetEngineLogForwarding(bool enabled) {
		m_EngineLogForwarding = enabled;
	}

	void ConsolePanel::OnImGuiRender() {
		using namespace UI;

		ScopedColor windowColors({
			{ImGuiCol_WindowBg, m_Style.WindowBg}
			});

		if (!BeginPanel("Console")) {
			EndPanel();
			return;
		}

		// ===== TAB BAR =====
		DrawTabBar();

		// ===== TOOLBAR =====
		DrawToolbar();

		// ===== TAB CONTENT =====
		if (m_ActiveTabIndex >= 0 && m_ActiveTabIndex < (int)m_Tabs.size()) {
			// Child with scroll for log content
			float inputHeight = m_Tabs[m_ActiveTabIndex]->IsTerminal() ? 0 : 30.0f;

			if (BeginChild("##TabContent", Size(0, -inputHeight), false)) {
				m_Tabs[m_ActiveTabIndex]->Draw();
			}
			EndChild();
		}

		// ===== COMMAND INPUT (only for non-terminal tabs) =====
		if (m_ActiveTabIndex >= 0 && m_ActiveTabIndex < (int)m_Tabs.size() && !m_Tabs[m_ActiveTabIndex]->IsTerminal()) {
			Separator();
			DrawCommandInput();
		}

		if (m_ShowCommandHelp) {
			DrawCommandHelp();
		}

		EndPanel();
	}

	void ConsolePanel::DrawTabBar() {
		using namespace UI;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		ImGui::PushStyleColor(ImGuiCol_Tab, m_Style.TabBg.ToImVec4());
		ImGui::PushStyleColor(ImGuiCol_TabActive, m_Style.TabActive.ToImVec4());
		ImGui::PushStyleColor(ImGuiCol_TabHovered, m_Style.TabHovered.ToImVec4());

		if (ImGui::BeginTabBar("##console_tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
			for (int i = 0; i < (int)m_Tabs.size(); i++) {
				bool open = true;
				ImGuiTabItemFlags flags = 0;

				// Add icon for terminal tabs
				std::string tabName = m_Tabs[i]->GetName();
				if (m_Tabs[i]->IsTerminal()) {
					tabName = ">_ " + tabName;
				}

				if (ImGui::BeginTabItem(tabName.c_str(), &open, flags)) {
					m_ActiveTabIndex = i;
					m_Tabs[i]->SetActive(true);
					ImGui::EndTabItem();
				}
				else {
					m_Tabs[i]->SetActive(false);
				}

				if (!open && m_Tabs.size() > 1) {
					m_Tabs.erase(m_Tabs.begin() + i);
					if (m_ActiveTabIndex >= i && m_ActiveTabIndex > 0)
						m_ActiveTabIndex--;
					i--;
				}
			}

			// Add tab button
			if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
				OpenPopup("AddTabPopup");
			}

			// Popup for adding new tabs
			if (BeginPopup("AddTabPopup")) {
				if (MenuItem("Log Tab")) {
					AddTab("Tab " + std::to_string(m_Tabs.size() + 1));
				}
				Separator();
				if (MenuItem("PowerShell Terminal")) {
					AddTerminalTab("PowerShell", GetTerminalWorkingDirectory(), TerminalType::PowerShell);
				}
				if (MenuItem("CMD Terminal")) {
					AddTerminalTab("CMD", GetTerminalWorkingDirectory(), TerminalType::CMD);
				}
				EndPopup();
			}

			ImGui::EndTabBar();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}

	void ConsolePanel::DrawToolbar() {
		using namespace UI;

		ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ScopedColor toolbarBg(ImGuiCol_ChildBg, m_Style.ToolbarBg);

		if (Button("Clear All", ButtonVariant::Default, ButtonSize::Small)) {
			for (auto& tab : m_Tabs)
				tab->Clear();
		}

		SameLine();

		if (Button("Export Logs", ButtonVariant::Default, ButtonSize::Small)) {
			AddLog("Log export not yet implemented", LogLevel::Warning, "System");
		}

		SameLine();

		ImGui::Checkbox("Help", &m_ShowCommandHelp);

		SameLine(0, 20);

		// Quick terminal buttons
		ScopedColor terminalBtnColors({
			{ImGuiCol_Button, Color(0.15f, 0.15f, 0.20f, 1.0f)},
			{ImGuiCol_ButtonHovered, Color(0.20f, 0.20f, 0.30f, 1.0f)}
			});

		if (ImGui::Button(">_ PowerShell")) {
			AddTerminalTab("PowerShell", GetTerminalWorkingDirectory(), TerminalType::PowerShell);
		}

		SameLine();

		if (ImGui::Button(">_ CMD")) {
			AddTerminalTab("CMD", GetTerminalWorkingDirectory(), TerminalType::CMD);
		}
	}

	void ConsolePanel::DrawCommandInput() {
		using namespace UI;

		ImGui::PushItemWidth(-1);

		ScopedColor inputColors({
			{ImGuiCol_FrameBg, m_Style.InputBg},
			{ImGuiCol_FrameBgActive, Color(0.26f, 0.59f, 0.98f, 0.50f)}
			});

		ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackHistory |
			ImGuiInputTextFlags_CallbackCompletion;

		auto inputCallback = [](ImGuiInputTextCallbackData* data) -> int {
			ConsolePanel* console = (ConsolePanel*)data->UserData;

			switch (data->EventFlag) {
			case ImGuiInputTextFlags_CallbackHistory: {
				int prevHistoryPos = console->m_HistoryPos;
				if (data->EventKey == ImGuiKey_UpArrow) {
					if (console->m_HistoryPos == -1)
						console->m_HistoryPos = (int)console->m_CommandHistory.size() - 1;
					else if (console->m_HistoryPos > 0)
						console->m_HistoryPos--;
				}
				else if (data->EventKey == ImGuiKey_DownArrow) {
					if (console->m_HistoryPos != -1) {
						console->m_HistoryPos++;
						if (console->m_HistoryPos >= console->m_CommandHistory.size())
							console->m_HistoryPos = -1;
					}
				}

				if (prevHistoryPos != console->m_HistoryPos) {
					const char* historyStr = (console->m_HistoryPos >= 0) ? console->m_CommandHistory[console->m_HistoryPos].c_str() : "";
					data->DeleteChars(0, data->BufTextLen);
					data->InsertChars(0, historyStr);
				}
				break;
			}
			case ImGuiInputTextFlags_CallbackCompletion: {
				console->AutoComplete();
				break;
			}
			}
			return 0;
			};

		if (ImGui::InputTextWithHint("##input", "Enter command...", m_InputBuffer, sizeof(m_InputBuffer), inputFlags, inputCallback, (void*)this)) {
			ProcessInput();
			m_ReclaimFocus = true;
		}

		ImGui::SetItemDefaultFocus();
		if (m_ReclaimFocus) {
			ImGui::SetKeyboardFocusHere(-1);
			m_ReclaimFocus = false;
		}

		ImGui::PopItemWidth();
	}

	void ConsolePanel::DrawCommandHelp() {
		using namespace UI;

		Separator();

		TextStyled("Available Commands:", TextVariant::Primary);
		AddSpacing(SpacingValues::SM);

		for (const auto& [name, cmd] : m_Commands) {
			ImGui::BulletText("%s - %s", name.c_str(), cmd.Description.c_str());
			if (!cmd.Usage.empty()) {
				Indent(16.0f);
				TextStyled(("Usage: " + cmd.Usage).c_str(), TextVariant::Muted);
				Unindent(16.0f);
			}
		}
	}

	void ConsolePanel::ProcessInput() {
		std::string command = m_InputBuffer;
		if (command.empty())
			return;

		// Add to history
		m_CommandHistory.push_back(command);
		m_HistoryPos = -1;

		// Echo command
		AddLog("> " + command, LogLevel::Info, "Command");

		// Clear input
		memset(m_InputBuffer, 0, sizeof(m_InputBuffer));

		// Execute
		ExecuteCommand(command);
	}

	void ConsolePanel::AutoComplete() {
		AddLog("Auto-complete not yet implemented", LogLevel::Info, "System");
	}

	std::vector<std::string> ConsolePanel::ParseCommandLine(const std::string& commandLine) {
		std::vector<std::string> args;
		std::stringstream ss(commandLine);
		std::string arg;

		while (ss >> arg) {
			args.push_back(arg);
		}

		return args;
	}

	std::filesystem::path ConsolePanel::GetTerminalWorkingDirectory() const {
		// Priority 1: Use active project's asset directory
		if (auto project = Project::GetActive()) {
			const auto& assetDir = project->GetAssetDirectory();
			if (!assetDir.empty() && std::filesystem::exists(assetDir)) {
				return assetDir;
			}
			const auto& projectDir = project->GetProjectDirectory();
			if (!projectDir.empty() && std::filesystem::exists(projectDir)) {
				return projectDir;
			}
		}

		// Priority 2: Use manually set project directory
		if (!m_ProjectDirectory.empty() && std::filesystem::exists(m_ProjectDirectory)) {
			return m_ProjectDirectory;
		}

		// Priority 3: Fallback to current working directory
		return std::filesystem::current_path();
	}

	void ConsolePanel::AddTab(const std::string& name) {
		m_Tabs.push_back(std::make_shared<ConsoleTab>(name));
		m_ActiveTabIndex = (int)m_Tabs.size() - 1;
	}

	void ConsolePanel::AddTerminalTab(const std::string& name, const std::filesystem::path& workingDirectory, TerminalType type) {
		m_Tabs.push_back(std::make_shared<TerminalTab>(name, workingDirectory, type));
		m_ActiveTabIndex = (int)m_Tabs.size() - 1;
	}

	void ConsolePanel::RemoveTab(const std::string& name) {
		for (int i = 0; i < (int)m_Tabs.size(); i++) {
			if (m_Tabs[i]->GetName() == name) {
				m_Tabs.erase(m_Tabs.begin() + i);
				if (m_ActiveTabIndex >= i && m_ActiveTabIndex > 0)
					m_ActiveTabIndex--;
				break;
			}
		}
	}

	void ConsolePanel::SwitchToTab(const std::string& name) {
		for (int i = 0; i < (int)m_Tabs.size(); i++) {
			if (m_Tabs[i]->GetName() == name) {
				m_ActiveTabIndex = i;
				break;
			}
		}
	}

	ConsoleTab* ConsolePanel::GetActiveTab() {
		if (m_ActiveTabIndex >= 0 && m_ActiveTabIndex < (int)m_Tabs.size())
			return m_Tabs[m_ActiveTabIndex].get();
		return nullptr;
	}

	void ConsolePanel::AddLog(const std::string& message, LogLevel level, const std::string& category) {
		if (auto* tab = GetActiveTab()) {
			tab->AddLog(message, level, category);
		}
	}

	void ConsolePanel::Clear() {
		for (auto& tab : m_Tabs)
			tab->Clear();
	}

	// ============================================================================
	// SCRIPT LOGGING API
	// ============================================================================

	void ConsolePanel::AddScriptLog(const std::string& message, LogLevel level) {
		AddLog(message, level, "Script");
	}

	// ============================================================================
	// COMPILATION LOGGING API
	// ============================================================================

	void ConsolePanel::AddCompileLog(const std::string& message, bool isError, bool isWarning) {
		LogLevel level = LogLevel::CompileSuccess;
		if (isError) level = LogLevel::CompileError;
		else if (isWarning) level = LogLevel::CompileWarning;

		AddLog(message, level, "Compiler");
	}

	void ConsolePanel::AddCompileStart(const std::string& scriptName) {
		AddLog("Compiling: " + scriptName + "...", LogLevel::CompileStart, "Compiler");
	}

	void ConsolePanel::AddCompileSuccess(const std::string& scriptName) {
		AddLog("Successfully compiled: " + scriptName, LogLevel::CompileSuccess, "Compiler");
	}

	void ConsolePanel::AddCompileError(const std::string& scriptName, const std::string& error) {
		AddLog("Compilation failed: " + scriptName, LogLevel::CompileError, "Compiler");
		AddLog("  Error: " + error, LogLevel::CompileError, "Compiler");
	}

	// ============================================================================
	// COMMAND SYSTEM
	// ============================================================================

	void ConsolePanel::RegisterCommand(const std::string& name, const std::string& description, const std::string& usage, CommandCallback callback) {
		ConsoleCommand cmd;
		cmd.Name = name;
		cmd.Description = description;
		cmd.Usage = usage;
		cmd.Callback = callback;
		m_Commands[name] = cmd;
	}

	void ConsolePanel::ExecuteCommand(const std::string& commandLine) {
		auto args = ParseCommandLine(commandLine);
		if (args.empty())
			return;

		std::string cmdName = args[0];
		args.erase(args.begin());

		auto it = m_Commands.find(cmdName);
		if (it != m_Commands.end()) {
			it->second.Callback(args);
		}
		else {
			AddLog("Unknown command: " + cmdName + ". Type 'help' for available commands.", LogLevel::Error, "Command");
		}
	}

	void ConsolePanel::RegisterBuiltInCommands() {
		RegisterCommand("help", "Show available commands", "help [command]",
			[this](const std::vector<std::string>& args) { CmdHelp(args); });

		RegisterCommand("clear", "Clear the console", "clear",
			[this](const std::vector<std::string>& args) { CmdClear(args); });

		RegisterCommand("echo", "Print a message", "echo <message>",
			[this](const std::vector<std::string>& args) { CmdEcho(args); });

		RegisterCommand("history", "Show command history", "history",
			[this](const std::vector<std::string>& args) { CmdHistory(args); });

		RegisterCommand("script", "Execute a script file", "script <filename>",
			[this](const std::vector<std::string>& args) { CmdScript(args); });

		RegisterCommand("exit", "Close the application", "exit",
			[this](const std::vector<std::string>& args) { CmdExit(args); });
	}

	void ConsolePanel::CmdHelp(const std::vector<std::string>& args) {
		if (args.empty()) {
			AddLog("Available commands:", LogLevel::Info, "Help");
			for (const auto& [name, cmd] : m_Commands) {
				AddLog("  " + name + " - " + cmd.Description, LogLevel::Info, "Help");
			}
		}
		else {
			auto it = m_Commands.find(args[0]);
			if (it != m_Commands.end()) {
				AddLog(it->second.Name + ": " + it->second.Description, LogLevel::Info, "Help");
				if (!it->second.Usage.empty())
					AddLog("Usage: " + it->second.Usage, LogLevel::Info, "Help");
			}
			else {
				AddLog("Unknown command: " + args[0], LogLevel::Error, "Help");
			}
		}
	}

	void ConsolePanel::CmdClear(const std::vector<std::string>& args) {
		if (auto* tab = GetActiveTab()) {
			tab->Clear();
		}
	}

	void ConsolePanel::CmdEcho(const std::vector<std::string>& args) {
		std::string message;
		for (const auto& arg : args) {
			message += arg + " ";
		}
		AddLog(message, LogLevel::Info, "Echo");
	}

	void ConsolePanel::CmdHistory(const std::vector<std::string>& args) {
		AddLog("Command history:", LogLevel::Info, "History");
		for (int i = 0; i < (int)m_CommandHistory.size(); i++) {
			AddLog("  " + std::to_string(i + 1) + ": " + m_CommandHistory[i], LogLevel::Info, "History");
		}
	}

	void ConsolePanel::CmdScript(const std::vector<std::string>& args) {
		if (args.empty()) {
			AddLog("Usage: script <filename>", LogLevel::Warning, "Script");
			return;
		}

		AddLog("Script execution not yet implemented: " + args[0], LogLevel::Warning, "Script");
	}

	void ConsolePanel::CmdExit(const std::vector<std::string>& args) {
		AddLog("Closing application...", LogLevel::Info, "System");
	}
}
