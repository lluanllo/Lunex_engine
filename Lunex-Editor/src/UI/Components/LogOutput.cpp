/**
 * @file LogOutput.cpp
 * @brief Implementation of LogOutput component
 */

#include "LogOutput.h"
#include <algorithm>

namespace Lunex::UI {

	LogOutput::LogOutput() {
		// Initialize default categories
		m_Categories.push_back("All");
		m_Categories.push_back("General");
		m_Categories.push_back("System");
		m_Categories.push_back("Scripting");
		m_Categories.push_back("Compiler");
		m_Categories.push_back("Renderer");
		m_Categories.push_back("Physics");
		m_Categories.push_back("Audio");
	}

	void LogOutput::AddMessage(const LogMessage& message) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		
		LogMessage msg = message;
		msg.Timestamp = ImGui::GetTime();
		m_Messages.push_back(msg);
		
		// Add category if new
		if (std::find(m_Categories.begin(), m_Categories.end(), message.Category) == m_Categories.end()) {
			m_Categories.push_back(message.Category);
		}
		
		// Limit message count
		if (m_Messages.size() > MAX_MESSAGES) {
			m_Messages.erase(m_Messages.begin(), m_Messages.begin() + (m_Messages.size() - MAX_MESSAGES));
		}
		
		m_ScrollToBottom = true;
	}

	void LogOutput::AddMessage(const std::string& message, LogLevel level, const std::string& category) {
		AddMessage(LogMessage(message, level, category));
	}

	void LogOutput::Clear() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Messages.clear();
	}

	void LogOutput::AddCategory(const std::string& category) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (std::find(m_Categories.begin(), m_Categories.end(), category) == m_Categories.end()) {
			m_Categories.push_back(category);
		}
	}

	void LogOutput::Render() {
		RenderFilterBar();
		RenderMessages();
	}

	void LogOutput::RenderFilterBar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, m_Style.FilterBarBg.ToImVec4());
		
		if (ImGui::BeginChild("##LogFilters", ImVec2(0, m_Style.FilterBarHeight), true, ImGuiWindowFlags_NoScrollbar)) {
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
			
			ImGui::PushID("LogLevelFilters");
			
			// Level toggles with colored icons
			ImGui::PushStyleColor(ImGuiCol_Text, m_Style.TraceColor.ToImVec4());
			ImGui::Checkbox("##Trace", &m_Filters.ShowTrace); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::Text("Trace"); ImGui::SameLine();
			
			ImGui::PushStyleColor(ImGuiCol_Text, m_Style.InfoColor.ToImVec4());
			ImGui::Checkbox("##Info", &m_Filters.ShowInfo); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::Text("Info"); ImGui::SameLine();
			
			ImGui::PushStyleColor(ImGuiCol_Text, m_Style.WarningColor.ToImVec4());
			ImGui::Checkbox("##Warning", &m_Filters.ShowWarning); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::Text("Warn"); ImGui::SameLine();
			
			ImGui::PushStyleColor(ImGuiCol_Text, m_Style.ErrorColor.ToImVec4());
			ImGui::Checkbox("##Error", &m_Filters.ShowError); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::Text("Error"); ImGui::SameLine();
			
			ImGui::PushStyleColor(ImGuiCol_Text, m_Style.ScriptInfoColor.ToImVec4());
			ImGui::Checkbox("##Script", &m_Filters.ShowScriptMessages); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::Text("Script"); ImGui::SameLine();
			
			ImGui::PushStyleColor(ImGuiCol_Text, m_Style.CompileSuccessColor.ToImVec4());
			ImGui::Checkbox("##Compile", &m_Filters.ShowCompileMessages); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::Text("Compile");
			
			ImGui::PopID();
			ImGui::PopStyleColor(); // FrameBgActive
			
			ImGui::SameLine(0, 20);
			
			// Category filter
			ImGui::SetNextItemWidth(120);
			if (ImGui::BeginCombo("##CategoryFilter", m_Filters.CategoryFilter.c_str())) {
				for (const auto& category : m_Categories) {
					bool isSelected = (m_Filters.CategoryFilter == category);
					if (ImGui::Selectable(category.c_str(), isSelected)) {
						m_Filters.CategoryFilter = category;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			
			ImGui::SameLine();
			
			// Search filter
			ImGui::SetNextItemWidth(180);
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
			ImGui::InputTextWithHint("##SearchFilter", "Search...", m_Filters.SearchFilter, sizeof(m_Filters.SearchFilter));
			ImGui::PopStyleColor();
			
			ImGui::SameLine();
			
			if (ImGui::Button("Clear")) {
				Clear();
			}
			
			ImGui::SameLine();
			
			ImGui::Checkbox("##AutoScroll", &m_Filters.AutoScroll);
			ImGui::SameLine();
			ImGui::Text("Auto");
		}
		ImGui::EndChild();
		
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	void LogOutput::RenderMessages() {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, m_Style.Background.ToImVec4());
		
		if (ImGui::BeginChild("##LogMessages", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, m_Style.MessageSpacing));
			
			std::lock_guard<std::mutex> lock(m_Mutex);
			
			for (size_t i = 0; i < m_Messages.size(); i++) {
				const auto& msg = m_Messages[i];
				if (PassesFilters(msg)) {
					DrawLogMessage(msg, (int)i);
				}
			}
			
			if (m_ScrollToBottom && m_Filters.AutoScroll) {
				ImGui::SetScrollHereY(1.0f);
				m_ScrollToBottom = false;
			}
			
			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
		
		ImGui::PopStyleColor();
	}

	void LogOutput::DrawLogMessage(const LogMessage& msg, int index) {
		ImGui::PushID(index);
		
		// Timestamp
		ImGui::PushStyleColor(ImGuiCol_Text, m_Style.TimestampColor.ToImVec4());
		ImGui::Text("[%.2f]", msg.Timestamp);
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		
		// Level icon with color
		Color levelColor = GetLogLevelColor(msg.Level);
		ImGui::PushStyleColor(ImGuiCol_Text, levelColor.ToImVec4());
		ImGui::Text("%s", GetLogLevelIcon(msg.Level));
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		
		// Category
		ImGui::PushStyleColor(ImGuiCol_Text, m_Style.CategoryColor.ToImVec4());
		ImGui::Text("[%s]", msg.Category.c_str());
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		
		// Message with level color
		ImGui::PushStyleColor(ImGuiCol_Text, levelColor.ToImVec4());
		ImGui::TextWrapped("%s", msg.Message.c_str());
		ImGui::PopStyleColor();
		
		ImGui::PopID();
	}

	bool LogOutput::PassesFilters(const LogMessage& msg) const {
		// Level filter
		bool levelPass = false;
		
		switch (msg.Level) {
			case LogLevel::Trace:
				levelPass = m_Filters.ShowTrace;
				break;
			case LogLevel::Info:
				levelPass = m_Filters.ShowInfo;
				break;
			case LogLevel::Warning:
				levelPass = m_Filters.ShowWarning;
				break;
			case LogLevel::Error:
			case LogLevel::Critical:
				levelPass = m_Filters.ShowError;
				break;
			case LogLevel::ScriptInfo:
			case LogLevel::ScriptWarning:
			case LogLevel::ScriptError:
				levelPass = m_Filters.ShowScriptMessages;
				break;
			case LogLevel::CompileStart:
			case LogLevel::CompileSuccess:
			case LogLevel::CompileError:
			case LogLevel::CompileWarning:
				levelPass = m_Filters.ShowCompileMessages;
				break;
		}
		
		if (!levelPass) return false;
		
		// Category filter
		if (m_Filters.CategoryFilter != "All" && msg.Category != m_Filters.CategoryFilter) {
			return false;
		}
		
		// Search filter
		if (m_Filters.SearchFilter[0] != '\0') {
			std::string msgLower = msg.Message;
			std::string filterLower = m_Filters.SearchFilter;
			std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
			std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
			
			if (msgLower.find(filterLower) == std::string::npos) {
				return false;
			}
		}
		
		return true;
	}

	Color LogOutput::GetLogLevelColor(LogLevel level) const {
		switch (level) {
			case LogLevel::Trace:          return m_Style.TraceColor;
			case LogLevel::Info:           return m_Style.InfoColor;
			case LogLevel::Warning:        return m_Style.WarningColor;
			case LogLevel::Error:          return m_Style.ErrorColor;
			case LogLevel::Critical:       return m_Style.CriticalColor;
			case LogLevel::ScriptInfo:     return m_Style.ScriptInfoColor;
			case LogLevel::ScriptWarning:  return m_Style.ScriptWarningColor;
			case LogLevel::ScriptError:    return m_Style.ScriptErrorColor;
			case LogLevel::CompileStart:   return m_Style.CompileStartColor;
			case LogLevel::CompileSuccess: return m_Style.CompileSuccessColor;
			case LogLevel::CompileError:   return m_Style.CompileErrorColor;
			case LogLevel::CompileWarning: return m_Style.CompileWarningColor;
		}
		return m_Style.InfoColor;
	}

	const char* LogOutput::GetLogLevelIcon(LogLevel level) const {
		switch (level) {
			case LogLevel::Trace:          return "[T]";
			case LogLevel::Info:           return "[I]";
			case LogLevel::Warning:        return "[W]";
			case LogLevel::Error:          return "[E]";
			case LogLevel::Critical:       return "[!]";
			case LogLevel::ScriptInfo:     return "[S]";
			case LogLevel::ScriptWarning:  return "[SW]";
			case LogLevel::ScriptError:    return "[SE]";
			case LogLevel::CompileStart:   return "[>>]";
			case LogLevel::CompileSuccess: return "[OK]";
			case LogLevel::CompileError:   return "[CE]";
			case LogLevel::CompileWarning: return "[CW]";
		}
		return "[?]";
	}

	const char* LogOutput::GetLogLevelName(LogLevel level) const {
		switch (level) {
			case LogLevel::Trace:          return "Trace";
			case LogLevel::Info:           return "Info";
			case LogLevel::Warning:        return "Warning";
			case LogLevel::Error:          return "Error";
			case LogLevel::Critical:       return "Critical";
			case LogLevel::ScriptInfo:     return "Script";
			case LogLevel::ScriptWarning:  return "Script Warning";
			case LogLevel::ScriptError:    return "Script Error";
			case LogLevel::CompileStart:   return "Compiling";
			case LogLevel::CompileSuccess: return "Compiled";
			case LogLevel::CompileError:   return "Compile Error";
			case LogLevel::CompileWarning: return "Compile Warning";
		}
		return "Unknown";
	}

} // namespace Lunex::UI
