#include "ConsolePanel.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <chrono>

namespace Lunex {

	ConsolePanel* ConsolePanel::s_Instance = nullptr;

	// ========== ConsoleTab Implementation ==========

	ConsoleTab::ConsoleTab(const std::string& name)
		: m_Name(name) {
		m_Categories.push_back("All");
		m_Categories.push_back("General");
		m_Categories.push_back("Scripting");
		m_Categories.push_back("Renderer");
		m_Categories.push_back("Physics");
		m_Categories.push_back("Audio");
	}

	void ConsoleTab::AddLog(const std::string& message, LogLevel level, const std::string& category) {
		ConsoleMessage msg(message, level, category);
		msg.Timestamp = ImGui::GetTime();
		m_Messages.push_back(msg);

		// Add category if it doesn't exist
		if (std::find(m_Categories.begin(), m_Categories.end(), category) == m_Categories.end()) {
			m_Categories.push_back(category);
		}

		m_ScrollToBottom = true;
	}

	void ConsoleTab::Clear() {
		m_Messages.clear();
	}

	void ConsoleTab::Draw() {
		// ===== FILTERS BAR (FIJA) =====
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		
		if (ImGui::BeginChild("##filters", ImVec2(0, 35), true, ImGuiWindowFlags_NoScrollbar)) {
			// Log level filters
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
			
			ImGui::Checkbox("Trace", &m_ShowTrace); ImGui::SameLine();
			ImGui::Checkbox("Info", &m_ShowInfo); ImGui::SameLine();
			ImGui::Checkbox("Warning", &m_ShowWarning); ImGui::SameLine();
			ImGui::Checkbox("Error", &m_ShowError); ImGui::SameLine();
			ImGui::Checkbox("Critical", &m_ShowCritical); ImGui::SameLine();
			
			ImGui::PopStyleColor();
			
			ImGui::SameLine(0, 20);
			ImGui::SetNextItemWidth(150);
			if (ImGui::BeginCombo("##Category", m_CategoryFilter.c_str())) {
				for (const auto& category : m_Categories) {
					bool isSelected = (m_CategoryFilter == category);
					if (ImGui::Selectable(category.c_str(), isSelected))
						m_CategoryFilter = category;
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			
			ImGui::SameLine();
			ImGui::SetNextItemWidth(200);
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
			ImGui::InputTextWithHint("##search", "?? Search...", m_SearchFilter, IM_ARRAYSIZE(m_SearchFilter));
			ImGui::PopStyleColor();
			
			ImGui::SameLine();
			if (ImGui::Button("Clear")) {
				Clear();
			}
			
			ImGui::SameLine();
			ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
		}
		ImGui::EndChild();
		
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		// ===== MESSAGES AREA CON SCROLL =====
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
		
		// BeginChild SIN especificar altura - tomará todo el espacio disponible
		if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
			
			for (int i = 0; i < m_Messages.size(); i++) {
				const auto& msg = m_Messages[i];
				if (PassesFilters(msg)) {
					DrawLogMessage(msg, i);
				}
			}
			
			if (m_ScrollToBottom && m_AutoScroll) {
				ImGui::SetScrollHereY(1.0f);
				m_ScrollToBottom = false;
			}
			
			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	void ConsoleTab::DrawLogMessage(const ConsoleMessage& msg, int index) {
		ImGui::PushID(index);
		
		// Timestamp
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::Text("[%.2f]", msg.Timestamp);
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		
		// Icon
		ImVec4 color = GetLogLevelColor(msg.Level);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::Text("%s", GetLogLevelIcon(msg.Level));
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		
		// Category
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
		ImGui::Text("[%s]", msg.Category.c_str());
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		
		// Message
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextWrapped("%s", msg.Message.c_str());
		ImGui::PopStyleColor();
		
		ImGui::PopID();
	}

	bool ConsoleTab::PassesFilters(const ConsoleMessage& msg) const {
		// Level filter
		bool levelPass = false;
		switch (msg.Level) {
			case LogLevel::Trace:    levelPass = m_ShowTrace; break;
			case LogLevel::Info:     levelPass = m_ShowInfo; break;
			case LogLevel::Warning:  levelPass = m_ShowWarning; break;
			case LogLevel::Error:    levelPass = m_ShowError; break;
			case LogLevel::Critical: levelPass = m_ShowCritical; break;
		}
		if (!levelPass) return false;

		// Category filter
		if (m_CategoryFilter != "All" && msg.Category != m_CategoryFilter)
			return false;

		// Search filter
		if (m_SearchFilter[0] != '\0') {
			std::string msgLower = msg.Message;
			std::string filterLower = m_SearchFilter;
			std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
			std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
			if (msgLower.find(filterLower) == std::string::npos)
				return false;
		}

		return true;
	}

	ImVec4 ConsoleTab::GetLogLevelColor(LogLevel level) const {
		switch (level) {
			case LogLevel::Trace:    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
			case LogLevel::Info:     return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
			case LogLevel::Warning:  return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
			case LogLevel::Error:    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
			case LogLevel::Critical: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		}
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	const char* ConsoleTab::GetLogLevelIcon(LogLevel level) const {
		switch (level) {
			case LogLevel::Trace:    return "[T]";
			case LogLevel::Info:     return "[I]";
			case LogLevel::Warning:  return "[W]";
			case LogLevel::Error:    return "[E]";
			case LogLevel::Critical: return "[!]";
		}
		return "[?]";
	}

	// ========== ConsolePanel Implementation ==========

	ConsolePanel::ConsolePanel() {
		s_Instance = this;
		
		// Create default tab
		AddTab("Main");
		
		// Register built-in commands
		RegisterBuiltInCommands();
		
		AddLog("Console initialized. Type 'help' for available commands.", LogLevel::Info, "System");
	}

	ConsolePanel& ConsolePanel::Get() {
		if (!s_Instance) {
			static ConsolePanel instance;
			return instance;
		}
		return *s_Instance;
	}

	void ConsolePanel::OnImGuiRender() {
		ImGui::Begin("Console");

		// ===== TAB BAR PRIMERO (SIEMPRE VISIBLE) =====
		DrawTabBar();
		
		// ===== TOOLBAR =====
		DrawToolbar();

		// ===== CONTENIDO DEL TAB ACTIVO CON SCROLL =====
		if (m_ActiveTabIndex >= 0 && m_ActiveTabIndex < m_Tabs.size()) {
			// Child con scroll para el contenido de mensajes
			ImGui::BeginChild("##TabContent", ImVec2(0, -30), false);
			m_Tabs[m_ActiveTabIndex]->Draw();
			ImGui::EndChild();
		}

		// ===== COMMAND INPUT AL FINAL (FIJO) =====
		ImGui::Separator();
		DrawCommandInput();

		if (m_ShowCommandHelp) {
			DrawCommandHelp();
		}

		ImGui::End();
	}

	void ConsolePanel::DrawTabBar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.40f));

		if (ImGui::BeginTabBar("##console_tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
			for (int i = 0; i < m_Tabs.size(); i++) {
				bool open = true;
				ImGuiTabItemFlags flags = 0;
				
				if (ImGui::BeginTabItem(m_Tabs[i]->GetName().c_str(), &open, flags)) {
					m_ActiveTabIndex = i;
					m_Tabs[i]->SetActive(true);
					ImGui::EndTabItem();
				} else {
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
				AddTab("Tab " + std::to_string(m_Tabs.size() + 1));
			}

			ImGui::EndTabBar();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}

	void ConsolePanel::DrawToolbar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		
		if (ImGui::Button("Clear All")) {
			for (auto& tab : m_Tabs)
				tab->Clear();
		}

		ImGui::SameLine();
		if (ImGui::Button("Export Logs")) {
			// TODO: Implement log export
			AddLog("Log export not yet implemented", LogLevel::Warning, "System");
		}

		ImGui::SameLine();
		ImGui::Checkbox("Show Help", &m_ShowCommandHelp);

		ImGui::PopStyleVar();
	}

	void ConsolePanel::DrawCommandInput() {
		ImGui::PushItemWidth(-1);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
		
		ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;
		
		auto inputCallback = [](ImGuiInputTextCallbackData* data) -> int {
			ConsolePanel* console = (ConsolePanel*)data->UserData;
			
			switch (data->EventFlag) {
				case ImGuiInputTextFlags_CallbackHistory: {
					// History navigation
					int prevHistoryPos = console->m_HistoryPos;
					if (data->EventKey == ImGuiKey_UpArrow) {
						if (console->m_HistoryPos == -1)
							console->m_HistoryPos = console->m_CommandHistory.size() - 1;
						else if (console->m_HistoryPos > 0)
							console->m_HistoryPos--;
					} else if (data->EventKey == ImGuiKey_DownArrow) {
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
					// Tab completion
					console->AutoComplete();
					break;
				}
			}
			return 0;
		};

		if (ImGui::InputTextWithHint("##input", "Enter command...", m_InputBuffer, IM_ARRAYSIZE(m_InputBuffer), inputFlags, inputCallback, (void*)this)) {
			ProcessInput();
			m_ReclaimFocus = true;
		}

		ImGui::SetItemDefaultFocus();
		if (m_ReclaimFocus) {
			ImGui::SetKeyboardFocusHere(-1);
			m_ReclaimFocus = false;
		}

		ImGui::PopStyleColor();
		ImGui::PopItemWidth();
	}

	void ConsolePanel::DrawCommandHelp() {
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Available Commands:");
		ImGui::Spacing();

		for (const auto& [name, cmd] : m_Commands) {
			ImGui::BulletText("%s - %s", name.c_str(), cmd.Description.c_str());
			if (!cmd.Usage.empty()) {
				ImGui::Indent();
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Usage: %s", cmd.Usage.c_str());
				ImGui::Unindent();
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
		// TODO: Implement auto-completion
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

	void ConsolePanel::AddTab(const std::string& name) {
		m_Tabs.push_back(std::make_shared<ConsoleTab>(name));
		m_ActiveTabIndex = m_Tabs.size() - 1;
	}

	void ConsolePanel::RemoveTab(const std::string& name) {
		for (int i = 0; i < m_Tabs.size(); i++) {
			if (m_Tabs[i]->GetName() == name) {
				m_Tabs.erase(m_Tabs.begin() + i);
				if (m_ActiveTabIndex >= i && m_ActiveTabIndex > 0)
					m_ActiveTabIndex--;
				break;
			}
		}
	}

	void ConsolePanel::SwitchToTab(const std::string& name) {
		for (int i = 0; i < m_Tabs.size(); i++) {
			if (m_Tabs[i]->GetName() == name) {
				m_ActiveTabIndex = i;
				break;
			}
		}
	}

	ConsoleTab* ConsolePanel::GetActiveTab() {
		if (m_ActiveTabIndex >= 0 && m_ActiveTabIndex < m_Tabs.size())
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

	void ConsolePanel::RegisterCommand(const std::string& name, const std::string& description, const std::string& usage, CommandCallback callback) {
		Command cmd;
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
		} else {
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
		} else {
			auto it = m_Commands.find(args[0]);
			if (it != m_Commands.end()) {
				AddLog(it->second.Name + ": " + it->second.Description, LogLevel::Info, "Help");
				if (!it->second.Usage.empty())
					AddLog("Usage: " + it->second.Usage, LogLevel::Info, "Help");
			} else {
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
		for (int i = 0; i < m_CommandHistory.size(); i++) {
			AddLog("  " + std::to_string(i + 1) + ": " + m_CommandHistory[i], LogLevel::Info, "History");
		}
	}

	void ConsolePanel::CmdScript(const std::vector<std::string>& args) {
		if (args.empty()) {
			AddLog("Usage: script <filename>", LogLevel::Warning, "Script");
			return;
		}
		
		// TODO: Implement script execution when scripting engine is ready
		AddLog("Script execution not yet implemented: " + args[0], LogLevel::Warning, "Script");
	}

	void ConsolePanel::CmdExit(const std::vector<std::string>& args) {
		AddLog("Closing application...", LogLevel::Info, "System");
		// TODO: Implement application close
	}
}
