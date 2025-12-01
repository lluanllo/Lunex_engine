#pragma once

#include <string>
#include <vector>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Lunex {

	class Process {
	public:
		Process();
		~Process();
		
		// Launch process with arguments
		bool Launch(const std::string& executable, const std::vector<std::string>& args = {});
		
		// Launch and capture output
		bool LaunchWithCapture(const std::string& executable, const std::vector<std::string>& args = {});
		
		// Check if process is running
		bool IsRunning() const;
		
		// Wait for process to exit
		int Wait(int timeoutMs = -1); // -1 = infinite
		
		// Terminate process
		void Terminate();
		
		// Read stdout line (if launched with capture)
		bool ReadStdoutLine(std::string& line);
		
		// Set callback for stdout
		void SetStdoutCallback(std::function<void(const std::string&)> callback) {
			m_StdoutCallback = callback;
		}
		
		// Get exit code
		int GetExitCode() const { return m_ExitCode; }
		
	private:
		void CleanUp();
		
#ifdef _WIN32
		HANDLE m_ProcessHandle = nullptr;
		HANDLE m_StdoutReadHandle = nullptr;
		HANDLE m_StdoutWriteHandle = nullptr;
#endif
		
		bool m_IsRunning = false;
		int m_ExitCode = 0;
		std::function<void(const std::string&)> m_StdoutCallback;
	};

}
