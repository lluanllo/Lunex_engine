#include "Process.h"
#include "Log/Log.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Lunex {

	Process::Process() {
	}

	Process::~Process() {
		CleanUp();
	}

#ifdef _WIN32

	bool Process::Launch(const std::string& executable, const std::vector<std::string>& args) {
		// Build command line
		std::string cmdLine = "\"" + executable + "\"";
		for (const auto& arg : args) {
			cmdLine += " \"" + arg + "\"";
		}
		
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
		
		// Launch process
		if (!CreateProcessA(
			nullptr,
			const_cast<char*>(cmdLine.c_str()),
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			nullptr,
			&si,
			&pi))
		{
			LNX_LOG_ERROR("Failed to launch process: {0}", executable);
			LNX_LOG_ERROR("Error code: {0}", GetLastError());
			return false;
		}
		
		m_ProcessHandle = pi.hProcess;
		m_IsRunning = true;
		
		CloseHandle(pi.hThread);
		
		LNX_LOG_INFO("Process launched: {0}", executable);
		return true;
	}

	bool Process::LaunchWithCapture(const std::string& executable, const std::vector<std::string>& args) {
		// Create pipes for stdout
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = nullptr;
		
		if (!CreatePipe(&m_StdoutReadHandle, &m_StdoutWriteHandle, &sa, 0)) {
			LNX_LOG_ERROR("Failed to create pipe for stdout");
			return false;
		}
		
		// Ensure the read handle is not inherited
		SetHandleInformation(m_StdoutReadHandle, HANDLE_FLAG_INHERIT, 0);
		
		// Build command line
		std::string cmdLine = "\"" + executable + "\"";
		for (const auto& arg : args) {
			cmdLine += " \"" + arg + "\"";
		}
		
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.hStdOutput = m_StdoutWriteHandle;
		si.hStdError = m_StdoutWriteHandle;
		si.dwFlags |= STARTF_USESTDHANDLES;
		ZeroMemory(&pi, sizeof(pi));
		
		// Launch process
		if (!CreateProcessA(
			nullptr,
			const_cast<char*>(cmdLine.c_str()),
			nullptr,
			nullptr,
			TRUE, // Inherit handles
			0,
			nullptr,
			nullptr,
			&si,
			&pi))
		{
			LNX_LOG_ERROR("Failed to launch process with capture: {0}", executable);
			LNX_LOG_ERROR("Error code: {0}", GetLastError());
			CloseHandle(m_StdoutReadHandle);
			CloseHandle(m_StdoutWriteHandle);
			return false;
		}
		
		m_ProcessHandle = pi.hProcess;
		m_IsRunning = true;
		
		CloseHandle(pi.hThread);
		CloseHandle(m_StdoutWriteHandle); // Close write handle in parent
		
		LNX_LOG_INFO("Process launched with capture: {0}", executable);
		return true;
	}

	bool Process::IsRunning() const {
		if (!m_ProcessHandle)
			return false;
		
		DWORD exitCode;
		if (GetExitCodeProcess(m_ProcessHandle, &exitCode)) {
			return exitCode == STILL_ACTIVE;
		}
		
		return false;
	}

	int Process::Wait(int timeoutMs) {
		if (!m_ProcessHandle)
			return -1;
		
		DWORD timeout = (timeoutMs < 0) ? INFINITE : static_cast<DWORD>(timeoutMs);
		DWORD result = WaitForSingleObject(m_ProcessHandle, timeout);
		
		if (result == WAIT_OBJECT_0) {
			DWORD exitCode;
			if (GetExitCodeProcess(m_ProcessHandle, &exitCode)) {
				m_ExitCode = static_cast<int>(exitCode);
				m_IsRunning = false;
			}
		}
		
		return m_ExitCode;
	}

	void Process::Terminate() {
		if (m_ProcessHandle && IsRunning()) {
			TerminateProcess(m_ProcessHandle, 1);
			m_IsRunning = false;
		}
	}

	bool Process::ReadStdoutLine(std::string& line) {
		if (!m_StdoutReadHandle)
			return false;
		
		line.clear();
		char buffer[4096];
		DWORD bytesRead;
		
		while (true) {
			if (!ReadFile(m_StdoutReadHandle, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
				if (GetLastError() == ERROR_BROKEN_PIPE) {
					// Pipe closed, process exited
					return false;
				}
				break;
			}
			
			if (bytesRead == 0)
				break;
			
			buffer[bytesRead] = '\0';
			line += buffer;
			
			// Check if we have a complete line
			size_t newlinePos = line.find('\n');
			if (newlinePos != std::string::npos) {
				std::string completeLine = line.substr(0, newlinePos);
				line = line.substr(newlinePos + 1);
				
				if (m_StdoutCallback) {
					m_StdoutCallback(completeLine);
				}
				
				return true;
			}
		}
		
		return false;
	}

	void Process::CleanUp() {
		if (m_ProcessHandle) {
			CloseHandle(m_ProcessHandle);
			m_ProcessHandle = nullptr;
		}
		
		if (m_StdoutReadHandle) {
			CloseHandle(m_StdoutReadHandle);
			m_StdoutReadHandle = nullptr;
		}
		
		m_IsRunning = false;
	}

#else
	// TODO: Implement for Linux/macOS using fork/exec and pipes
	bool Process::Launch(const std::string& executable, const std::vector<std::string>& args) {
		LNX_LOG_ERROR("Process::Launch not implemented for this platform");
		return false;
	}

	bool Process::LaunchWithCapture(const std::string& executable, const std::vector<std::string>& args) {
		LNX_LOG_ERROR("Process::LaunchWithCapture not implemented for this platform");
		return false;
	}

	bool Process::IsRunning() const {
		return false;
	}

	int Process::Wait(int timeoutMs) {
		return -1;
	}

	void Process::Terminate() {
	}

	bool Process::ReadStdoutLine(std::string& line) {
		return false;
	}

	void Process::CleanUp() {
	}
#endif

}
