#include "stpch.h"
#include "ScriptBuild.h"

#include "Core/JobSystem/JobSystem.h"
#include "Project/Project.h"
#include "Log/Log.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Lunex::ScriptBuild {

	namespace fs = std::filesystem;

	// ------------------------------------------------------------------ configuration
	static const char* Configuration() {
#if defined(LN_DEBUG)
		return "Debug";
#elif defined(LN_DIST)
		return "Dist";
#else
		return "Release";
#endif
	}

	// ------------------------------------------------------------------ status registry
	struct ScriptEntry {
		std::mutex BuildMutex;           // serializes compiles of the same script
		ScriptBuildStatus Status;
	};
	static std::mutex s_RegistryMutex;
	static std::unordered_map<std::string, std::shared_ptr<ScriptEntry>> s_Registry;

	static std::string Key(const fs::path& p) {
		std::error_code ec;
		auto canonical = fs::weakly_canonical(p, ec);
		std::string s = (ec ? p : canonical).lexically_normal().string();
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		return s;
	}

	static std::shared_ptr<ScriptEntry> GetEntry(const fs::path& script) {
		std::scoped_lock lock(s_RegistryMutex);
		auto& e = s_Registry[Key(script)];
		if (!e) e = std::make_shared<ScriptEntry>();
		return e;
	}

	static void SetStatus(const std::shared_ptr<ScriptEntry>& e, const ScriptBuildStatus& s) {
		std::scoped_lock lock(s_RegistryMutex);
		e->Status = s;
	}

	ScriptBuildStatus GetStatus(const fs::path& scriptFile) {
		auto e = GetEntry(scriptFile);
		std::scoped_lock lock(s_RegistryMutex);
		return e->Status;
	}

	// ------------------------------------------------------------------ paths
	fs::path ResolveScriptPath(const std::string& scriptPath) {
		fs::path p(scriptPath);
		std::error_code ec;
		if (p.is_absolute()) return p.lexically_normal();

		if (auto project = Project::GetActive()) {
			fs::path candidate = project->GetAssetDirectory() / p;
			if (fs::exists(candidate, ec)) return fs::absolute(candidate, ec).lexically_normal();
		}
		// Legacy: paths stored relative to the editor's "assets" folder
		fs::path legacy = fs::path("assets") / p;
		return fs::absolute(legacy, ec).lexically_normal();
	}

	fs::path GetDllPath(const fs::path& scriptFile) {
		return scriptFile.parent_path() / "bin" / Configuration() / (scriptFile.stem().string() + ".dll");
	}

	static fs::path ExecutableDir() {
#ifdef _WIN32
		wchar_t buf[MAX_PATH];
		DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
		if (n > 0) return fs::path(std::wstring(buf, n)).parent_path();
#endif
		return fs::current_path();
	}

	// Root containing Lunex-ScriptCore/, Lunex/ and vendor/
	static fs::path FindEngineRoot(const fs::path& scriptDir) {
		static std::mutex s_Mutex;
		static fs::path s_Cached;
		{
			std::scoped_lock lock(s_Mutex);
			if (!s_Cached.empty()) return s_Cached;
		}

		std::error_code ec;
		for (fs::path start : { scriptDir, fs::current_path(ec), ExecutableDir() }) {
			fs::path dir = fs::absolute(start, ec);
			for (int i = 0; i < 12 && !dir.empty(); i++) {
				if (fs::exists(dir / "Lunex-ScriptCore" / "src" / "LunexScriptingAPI.h", ec)) {
					std::scoped_lock lock(s_Mutex);
					s_Cached = dir;
					return dir;
				}
				fs::path parent = dir.parent_path();
				if (parent == dir) break;
				dir = parent;
			}
		}
		return {};
	}

	static std::string RunAndCapture(const std::string& command) {
		std::string output;
		FILE* pipe = _popen(command.c_str(), "r");
		if (!pipe) return output;
		std::array<char, 512> buf{};
		while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) output += buf.data();
		_pclose(pipe);
		return output;
	}

	// vcvars64.bat of the newest Visual Studio with the C++ toolset
	static fs::path FindVcVars() {
		static std::once_flag s_Once;
		static fs::path s_VcVars;
		std::call_once(s_Once, []() {
			std::error_code ec;
			const char* pf86 = std::getenv("ProgramFiles(x86)");
			fs::path vswhere = fs::path(pf86 ? pf86 : "C:\\Program Files (x86)") / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
			if (fs::exists(vswhere, ec)) {
				std::string cmd = "\"\"" + vswhere.string() + "\" -latest -products * "
					"-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath\"";
				std::string out = RunAndCapture(cmd);
				while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
				auto nl = out.find_first_of("\r\n");
				if (nl != std::string::npos) out = out.substr(0, nl);
				fs::path candidate = fs::path(out) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
				if (!out.empty() && fs::exists(candidate, ec)) { s_VcVars = candidate; return; }
			}

			// Fallback: well-known install locations (newest first)
			for (const char* version : { "18", "2022", "2019" }) {
				for (const char* root : { "C:\\Program Files\\Microsoft Visual Studio\\", "C:\\Program Files (x86)\\Microsoft Visual Studio\\" }) {
					for (const char* edition : { "Community", "Professional", "Enterprise", "BuildTools" }) {
						fs::path candidate = fs::path(root) / version / edition / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
						if (fs::exists(candidate, ec)) { s_VcVars = candidate; return; }
					}
				}
			}
		});
		return s_VcVars;
	}

	static fs::file_time_type NewestApiTime(const fs::path& engineRoot) {
		std::error_code ec;
		fs::file_time_type newest{};
		for (const char* f : { "LunexScriptingAPI.h", "LunexScriptingAPI.cpp" }) {
			auto t = fs::last_write_time(engineRoot / "Lunex-ScriptCore" / "src" / f, ec);
			if (!ec && t > newest) newest = t;
		}
		return newest;
	}

	bool NeedsCompile(const fs::path& scriptFile) {
		std::error_code ec;
		fs::path dll = GetDllPath(scriptFile);
		if (!fs::exists(dll, ec)) return true;
		auto dllTime = fs::last_write_time(dll, ec);
		if (ec) return true;
		auto scriptTime = fs::last_write_time(scriptFile, ec);
		if (!ec && scriptTime > dllTime) return true;
		fs::path root = FindEngineRoot(scriptFile.parent_path());
		return !root.empty() && NewestApiTime(root) > dllTime;
	}

	static bool IsErrorLine(const std::string& line) {
		return line.find(": error ") != std::string::npos ||
			line.find(": fatal error ") != std::string::npos ||
			line.find("error LNK") != std::string::npos ||
			line.rfind("ERROR:", 0) == 0;
	}

	bool Compile(const fs::path& scriptFile, std::string& outDllPath) {
		auto entry = GetEntry(scriptFile);
		std::scoped_lock buildLock(entry->BuildMutex);   // one compile per script at a time

		const fs::path dll = GetDllPath(scriptFile);
		std::error_code ec;

		if (!fs::exists(scriptFile, ec)) {
			ScriptBuildStatus st; st.State = ScriptBuildState::Failed;
			st.Errors.push_back("Script file not found: " + scriptFile.string());
			SetStatus(entry, st);
			LNX_LOG_ERROR("[ScriptBuild] {0}", st.Errors.back());
			return false;
		}

		if (!NeedsCompile(scriptFile)) {
			outDllPath = dll.string();
			ScriptBuildStatus st = GetStatus(scriptFile);
			if (st.State != ScriptBuildState::Succeeded) { st = {}; st.State = ScriptBuildState::Succeeded; st.DllPath = outDllPath; SetStatus(entry, st); }
			return true;
		}

		{
			ScriptBuildStatus st; st.State = ScriptBuildState::Compiling;
			SetStatus(entry, st);
		}
		auto start = std::chrono::steady_clock::now();

		auto fail = [&](const std::vector<std::string>& errors) {
			ScriptBuildStatus st;
			st.State = ScriptBuildState::Failed;
			st.Errors = errors;
			st.Seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
			SetStatus(entry, st);
			for (const auto& e : errors) LNX_LOG_ERROR("[ScriptBuild] {0}", e);
			return false;
		};

		const fs::path vcvars = FindVcVars();
		if (vcvars.empty())
			return fail({ "Visual Studio with the C++ toolset was not found (install the 'Desktop development with C++' workload)" });

		const fs::path root = FindEngineRoot(scriptFile.parent_path());
		if (root.empty())
			return fail({ "Engine headers (Lunex-ScriptCore/src) not found from the script, working directory or executable" });

		const std::string name = scriptFile.stem().string();
		const fs::path binDir = dll.parent_path();
		const fs::path intDir = scriptFile.parent_path() / "bin-int" / Configuration() / name;   // per script
		fs::create_directories(binDir, ec);
		fs::create_directories(intDir, ec);

		// The previous DLL must be replaceable. Loaded copies are separate files (MakeLoadableCopy).
		if (fs::exists(dll, ec)) {
			fs::remove(dll, ec);
			if (ec) return fail({ "Cannot overwrite " + dll.string() + " (" + ec.message() + "). Is it loaded by another process?" });
		}

		const fs::path apiCpp = root / "Lunex-ScriptCore" / "src" / "LunexScriptingAPI.cpp";
		std::string flags = "/LD /EHsc /std:c++20 /utf-8 /nologo /DLUNEX_SCRIPT_EXPORT /DGLM_ENABLE_EXPERIMENTAL";
		flags += (std::string(Configuration()) == "Debug") ? " /MDd /Zi /Od /DLN_DEBUG" : " /MD /O2 /DLN_RELEASE";

		const fs::path bat = intDir / "compile.bat";
		{
			std::ofstream out(bat);
			out << "@echo off\n";
			out << "call \"" << vcvars.string() << "\" >nul 2>&1\n";
			out << "if errorlevel 1 (echo ERROR: could not initialize the MSVC environment & exit /b 1)\n";
			out << "cl.exe " << flags
				<< " /I\"" << (root / "Lunex-ScriptCore" / "src").string() << "\""
				<< " /I\"" << (root / "Lunex" / "src").string() << "\""
				<< " /I\"" << (root / "vendor" / "spdlog" / "include").string() << "\""
				<< " /I\"" << (root / "vendor" / "glm").string() << "\""
				<< " \"" << scriptFile.string() << "\"";
			if (fs::exists(apiCpp, ec)) out << " \"" << apiCpp.string() << "\"";
			out << " /Fe:\"" << dll.string() << "\" /Fo:\"" << intDir.string() << "\\\\\""
				<< " /Fd:\"" << (intDir / (name + ".pdb")).string() << "\"\n";
			out << "exit /b %errorlevel%\n";
		}

		LNX_LOG_INFO("[ScriptBuild] Compiling {0}...", scriptFile.filename().string());

		std::vector<std::string> errors;
		std::string command = "\"\"" + bat.string() + "\" 2>&1\"";
		FILE* pipe = _popen(command.c_str(), "r");
		if (!pipe) return fail({ "Failed to start the compiler" });

		std::array<char, 1024> buf{};
		while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
			std::string line = buf.data();
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
			if (line.empty()) continue;
			if (IsErrorLine(line)) errors.push_back(line);
			else if (line.find(": warning ") != std::string::npos) LNX_LOG_WARN("[ScriptBuild] {0}", line);
		}
		int exitCode = _pclose(pipe);

		if (exitCode != 0 || !errors.empty() || !fs::exists(dll, ec)) {
			if (errors.empty()) errors.push_back("Compiler exited with code " + std::to_string(exitCode));
			return fail(errors);
		}

		ScriptBuildStatus st;
		st.State = ScriptBuildState::Succeeded;
		st.DllPath = dll.string();
		st.Seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
		SetStatus(entry, st);
		outDllPath = st.DllPath;
		LNX_LOG_INFO("[ScriptBuild] {0} compiled in {1:.1f}s", scriptFile.filename().string(), st.Seconds);
		return true;
	}

	void CompileAsync(const fs::path& scriptFile) {
		if (GetStatus(scriptFile).State == ScriptBuildState::Compiling) return;
		if (!JobSystem::IsInitialized()) { std::string dll; Compile(scriptFile, dll); return; }

		{ // mark immediately so repeated calls (e.g. every frame) don't queue duplicates
			auto e = GetEntry(scriptFile);
			ScriptBuildStatus st; st.State = ScriptBuildState::Compiling;
			SetStatus(e, st);
		}
		fs::path copy = scriptFile;
		JobSystem::Get().Schedule([copy]() {
			std::string dll;
			// Compile() re-checks staleness; reset the "Compiling" mark so it actually runs
			Compile(copy, dll);
		}, nullptr, nullptr, JobPriority::Low);
	}

	// ------------------------------------------------------------------ loadable copies
	std::string MakeLoadableCopy(const std::string& dllPath) {
		static std::atomic<uint32_t> s_Counter{ 0 };
		std::error_code ec;
		fs::path src(dllPath);
		fs::path dir = src.parent_path() / "loaded";
		fs::create_directories(dir, ec);
		uint32_t n = ++s_Counter;
#ifdef _WIN32
		DWORD pid = GetCurrentProcessId();
#else
		uint32_t pid = 0;
#endif
		fs::path dst = dir / (src.stem().string() + "_" + std::to_string(pid) + "_" + std::to_string(n) + ".dll");
		fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
		if (ec) {
			LNX_LOG_WARN("[ScriptBuild] Could not copy {0} for loading ({1}); loading it in place", src.filename().string(), ec.message());
			return dllPath;
		}
		return dst.string();
	}

	void CleanupLoadableCopies(const std::string& dllPath) {
		std::error_code ec;
		fs::path dir = fs::path(dllPath).parent_path() / "loaded";
		if (!fs::exists(dir, ec)) return;
		const std::string prefix = fs::path(dllPath).stem().string() + "_";
		for (auto& f : fs::directory_iterator(dir, ec)) {
			if (f.path().filename().string().rfind(prefix, 0) == 0) fs::remove(f.path(), ec); // locked copies are skipped
		}
	}

} // namespace Lunex::ScriptBuild
