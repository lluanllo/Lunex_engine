#include "stpch.h"
#include "Utils/PlatformUtils.h"

#include <commdlg.h>
#include <shlobj.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "Core/Application.h"

namespace Lunex {
	std::string FileDialogs::OpenFile(const char* filter) {
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn) == TRUE) {
			return ofn.lpstrFile;
		}
		return std::string();
	}

	bool RestartApplication(const std::vector<std::string>& args) {
		char exe[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, exe, MAX_PATH);
		std::string cmd = std::string("\"") + exe + "\"";
		for (const auto& a : args) cmd += " \"" + a + "\"";
		char cwd[MAX_PATH] = {};
		GetCurrentDirectoryA(MAX_PATH, cwd);
		STARTUPINFOA si{};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};
		std::vector<char> buffer(cmd.begin(), cmd.end());
		buffer.push_back('\0');
		if (!CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, cwd, &si, &pi)) return false;
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return true;
	}

	std::vector<std::string> FileDialogs::OpenFiles(const char* filter, const char* title) {
		std::vector<std::string> files;
		std::vector<char> buffer(32 * 1024, '\0');
		OPENFILENAMEA ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = buffer.data();
		ofn.nMaxFile = static_cast<DWORD>(buffer.size());
		ofn.lpstrTitle = title;
		ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn) != TRUE) return files;

		// Single file: full path. Several: directory \0 file1 \0 file2 \0 \0
		const std::string first = buffer.data();
		const char* p = buffer.data() + first.size() + 1;
		if (*p == '\0') { files.push_back(first); return files; }
		while (*p) { files.push_back(first + "\\" + p); p += std::strlen(p) + 1; }
		return files;
	}

	std::string FileDialogs::SaveFile(const char* filter) {
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetSaveFileNameA(&ofn) == TRUE) {
			return ofn.lpstrFile;
		}
		return std::string();
	}

	std::string FileDialogs::SelectFolder() {
		BROWSEINFOA bi = { 0 };
		bi.lpszTitle = "Select Project Location";
		bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
		bi.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		
		LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
		if (pidl != 0) {
			CHAR path[MAX_PATH];
			if (SHGetPathFromIDListA(pidl, path)) {
				IMalloc* imalloc = 0;
				if (SUCCEEDED(SHGetMalloc(&imalloc))) {
					imalloc->Free(pidl);
					imalloc->Release();
				}
				return std::string(path);
			}
		}
		return std::string();
	}
}