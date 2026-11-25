#pragma once

/**
 * @file ScriptBuild.h
 * @brief C++ script compilation pipeline (MSVC) used by Play mode and the editor.
 *
 * - Resolves script paths (absolute, project asset dir, or legacy "assets/" relative)
 * - Locates MSVC through vswhere (any VS version) with a fallback list
 * - Locates the engine headers from the script, the working dir or the executable dir
 * - Per-script temp/obj folders, so scripts can compile in parallel safely
 * - Recompiles when the script OR the scripting API changed
 * - Thread-safe per script; keeps a status (compiling / ok / errors) for the UI
 */

#include <filesystem>
#include <string>
#include <vector>

namespace Lunex {

	enum class ScriptBuildState {
		Idle,        // never compiled in this session
		Compiling,
		Succeeded,
		Failed
	};

	struct ScriptBuildStatus {
		ScriptBuildState State = ScriptBuildState::Idle;
		std::vector<std::string> Errors;   // compiler error lines
		std::string DllPath;
		float Seconds = 0.0f;              // duration of the last compile
	};

	namespace ScriptBuild {

		// Turn a ScriptComponent path into an absolute file path
		std::filesystem::path ResolveScriptPath(const std::string& scriptPath);

		// Where the DLL for a script lives
		std::filesystem::path GetDllPath(const std::filesystem::path& scriptFile);

		// True if the DLL is missing or older than the script / scripting API
		bool NeedsCompile(const std::filesystem::path& scriptFile);

		// Blocking compile (skips if up to date). Safe to call from worker threads.
		bool Compile(const std::filesystem::path& scriptFile, std::string& outDllPath);

		// Fire-and-forget compile on the job system (no-op if already compiling)
		void CompileAsync(const std::filesystem::path& scriptFile);

		ScriptBuildStatus GetStatus(const std::filesystem::path& scriptFile);

		// Copy a DLL to a unique file and return that path, so the original can be
		// recompiled while the copy is loaded (hot reload / compile during Play)
		std::string MakeLoadableCopy(const std::string& dllPath);
		void CleanupLoadableCopies(const std::string& dllPath);

	} // namespace ScriptBuild

} // namespace Lunex
