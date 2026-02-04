#include "stpch.h"
#include "ScriptCompiler.h"
#include <cstdio>
#include <regex>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Lunex {

    CompileResult ScriptCompiler::Compile(const std::string& scriptPath, const CompileOptions& options) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        ReportProgress("Starting compilation: " + scriptPath, 0.0f);
        
        CompileResult result;
        
        #ifdef _WIN32
            result = CompileWindows(scriptPath, options);
        #else
            result = CompileUnix(scriptPath, options);
        #endif
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.compileTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        if (result.success) {
            ReportProgress("Compilation successful: " + result.outputPath, 1.0f);
        } else {
            ReportProgress("Compilation failed", 1.0f);
        }
        
        return result;
    }

    std::vector<CompileResult> ScriptCompiler::CompileBatch(
        const std::vector<std::string>& scriptPaths,
        const CompileOptions& options
    ) {
        std::vector<CompileResult> results;
        results.reserve(scriptPaths.size());
        
        float totalScripts = static_cast<float>(scriptPaths.size());
        
        for (size_t i = 0; i < scriptPaths.size(); i++) {
            ReportProgress("Compiling script " + std::to_string(i + 1) + "/" + std::to_string(scriptPaths.size()),
                          static_cast<float>(i) / totalScripts);
            
            results.push_back(Compile(scriptPaths[i], options));
        }
        
        return results;
    }

    bool ScriptCompiler::NeedsRecompile(const std::string& scriptPath, const std::string& dllPath) {
        namespace fs = std::filesystem;
        
        if (!fs::exists(dllPath)) {
            return true;
        }
        
        if (!fs::exists(scriptPath)) {
            return false; // Can't compile what doesn't exist
        }
        
        auto dllTime = fs::last_write_time(dllPath);
        auto scriptTime = fs::last_write_time(scriptPath);
        
        return scriptTime > dllTime;
    }

    std::string ScriptCompiler::GetDLLPath(const std::string& scriptPath, const CompileOptions& options) {
        namespace fs = std::filesystem;
        
        fs::path script(scriptPath);
        fs::path scriptDir = script.parent_path();
        fs::path scriptName = script.stem();
        
        fs::path outputDir = scriptDir / "bin" / options.configuration;
        
        return (outputDir / (scriptName.string() + ".dll")).string();
    }

    bool ScriptCompiler::DetectVisualStudio() {
        if (m_VSDetected) return !m_CLPath.empty();
        
        m_VSDetected = true;
        
        #ifndef _WIN32
            return false;
        #endif
        
        std::vector<std::string> vsBasePaths = {
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Professional",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Enterprise",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools",
            "C:\\Program Files\\Microsoft Visual Studio\\2019\\Community",
            "C:\\Program Files\\Microsoft Visual Studio\\2019\\Professional",
            "C:\\Program Files\\Microsoft Visual Studio\\2019\\Enterprise",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community",
        };
        
        namespace fs = std::filesystem;
        
        for (const auto& basePath : vsBasePaths) {
            fs::path candidate(basePath);
            fs::path vcvarsCandidate = candidate / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
            
            if (fs::exists(vcvarsCandidate)) {
                m_VSPath = basePath;
                m_VCVarsPath = vcvarsCandidate.string();
                
                // Find cl.exe
                fs::path msvcDir = candidate / "VC" / "Tools" / "MSVC";
                if (fs::exists(msvcDir)) {
                    for (auto& entry : fs::directory_iterator(msvcDir)) {
                        if (entry.is_directory()) {
                            fs::path clExe = entry.path() / "bin" / "Hostx64" / "x64" / "cl.exe";
                            if (fs::exists(clExe)) {
                                m_CLPath = clExe.string();
                                LNX_LOG_INFO("[ScriptCompiler] Found Visual Studio at: {0}", m_VSPath);
                                LNX_LOG_INFO("[ScriptCompiler] Found cl.exe at: {0}", m_CLPath);
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        LNX_LOG_ERROR("[ScriptCompiler] Could not detect Visual Studio installation");
        return false;
    }

    CompileResult ScriptCompiler::CompileWindows(const std::string& scriptPath, const CompileOptions& options) {
        CompileResult result;
        namespace fs = std::filesystem;
        
        // Detect Visual Studio
        if (!DetectVisualStudio()) {
            result.errors = "Visual Studio not detected. Please install VS 2019 or 2022.";
            return result;
        }
        
        ReportProgress("Preparing compilation...", 0.1f);
        
        // Set up paths
        fs::path fullScriptPath = fs::path("assets") / scriptPath;
        if (!fs::exists(fullScriptPath)) {
            fullScriptPath = scriptPath; // Try as absolute path
        }
        
        if (!fs::exists(fullScriptPath)) {
            result.errors = "Script file not found: " + scriptPath;
            return result;
        }
        
        fs::path scriptDir = fullScriptPath.parent_path();
        fs::path scriptName = fullScriptPath.stem();
        
        fs::path binDir = scriptDir / "bin" / options.configuration;
        fs::path objDir = scriptDir / "bin-int" / options.configuration;
        
        fs::create_directories(binDir);
        fs::create_directories(objDir);
        
        fs::path dllPath = binDir / (scriptName.string() + ".dll");
        result.outputPath = dllPath.string();
        
        ReportProgress("Finding include paths...", 0.2f);
        
        // Find include paths
        std::string scriptCoreInclude;
        std::string lunexInclude;
        std::string spdlogInclude;
        std::string glmInclude;
        
        fs::path searchDir = scriptDir;
        for (int i = 0; i < 10; i++) {
            fs::path candidate = searchDir / "Lunex-ScriptCore" / "src";
            if (fs::exists(candidate)) {
                scriptCoreInclude = candidate.string();
                lunexInclude = (searchDir / "Lunex" / "src").string();
                spdlogInclude = (searchDir / "vendor" / "spdlog" / "include").string();
                glmInclude = (searchDir / "vendor" / "glm").string();
                break;
            }
            searchDir = searchDir.parent_path();
        }
        
        if (scriptCoreInclude.empty()) {
            result.errors = "Could not find Lunex-ScriptCore include path";
            return result;
        }
        
        // Find GLM if not found
        if (!fs::exists(glmInclude)) {
            searchDir = scriptDir;
            for (int i = 0; i < 10; i++) {
                fs::path glmCandidate = searchDir / "vendor" / "glm";
                if (fs::exists(glmCandidate)) {
                    glmInclude = glmCandidate.string();
                    break;
                }
                searchDir = searchDir.parent_path();
            }
        }
        
        ReportProgress("Generating compile script...", 0.3f);
        
        // Create batch file
        fs::path tempBatPath = scriptDir / "temp_compile.bat";
        
        std::ofstream batFile(tempBatPath);
        if (!batFile.is_open()) {
            result.errors = "Failed to create temporary batch file";
            return result;
        }
        
        batFile << "@echo off\n";
        batFile << "REM Auto-generated compile script - Lunex Script Compiler\n";
        batFile << "call \"" << m_VCVarsPath << "\" >nul 2>&1\n";
        batFile << "if errorlevel 1 (\n";
        batFile << "    echo ERROR: Failed to setup Visual Studio environment\n";
        batFile << "    exit /b 1\n";
        batFile << ")\n\n";
        
        // Compiler flags
        std::string compilerFlags = "/LD /EHsc /std:c++20 /utf-8 /nologo";
        if (options.configuration == "Debug") {
            compilerFlags += " /MDd /Zi /Od /DLUNEX_SCRIPT_EXPORT /DLN_DEBUG";
        } else {
            compilerFlags += " /MD /O2 /DLUNEX_SCRIPT_EXPORT /DLN_RELEASE";
        }
        
        // Include paths
        std::string includes =
            " /I\"" + scriptCoreInclude + "\"" +
            " /I\"" + lunexInclude + "\"" +
            " /I\"" + spdlogInclude + "\"" +
            " /I\"" + glmInclude + "\"";
        
        // Add custom include paths
        for (const auto& inc : options.includePaths) {
            includes += " /I\"" + inc + "\"";
        }
        
        // Add defines
        for (const auto& def : options.defines) {
            compilerFlags += " /D" + def;
        }
        
        std::string outputDll = "/Fe:\"" + dllPath.string() + "\"";
        std::string outputObj = "/Fo:\"" + objDir.string() + "\\\\\"";
        
        // Find LunexScriptingAPI.cpp
        fs::path apiCppPath = fs::path(scriptCoreInclude) / "LunexScriptingAPI.cpp";
        
        batFile << "cl.exe " << compilerFlags << includes;
        batFile << " \"" << fullScriptPath.string() << "\"";
        if (fs::exists(apiCppPath)) {
            batFile << " \"" << apiCppPath.string() << "\"";
        }
        batFile << " " << outputDll << " " << outputObj << " 2>&1\n";
        batFile << "exit /b %errorlevel%\n";
        
        batFile.close();
        
        ReportProgress("Compiling...", 0.5f);
        
        // Execute compilation
        std::string batCommand = "\"" + tempBatPath.string() + "\" 2>&1";
        result.output = ExecuteCommand(batCommand);
        
        ReportProgress("Parsing output...", 0.9f);
        
        // Parse output
        ParseCompilerOutput(result.output, result);
        
        // Clean up
        fs::remove(tempBatPath);
        
        // Check if DLL was created
        if (fs::exists(dllPath)) {
            result.success = result.errorMessages.empty();
            if (result.success) {
                LNX_LOG_INFO("[ScriptCompiler] Compiled: {0}", dllPath.string());
            }
        } else {
            result.success = false;
            if (result.errors.empty()) {
                result.errors = "Compilation failed - DLL not created";
            }
        }
        
        return result;
    }

    CompileResult ScriptCompiler::CompileUnix(const std::string& scriptPath, const CompileOptions& options) {
        CompileResult result;
        
        // Unix compilation using g++ or clang++
        // Similar structure to Windows but with different flags
        
        result.errors = "Unix compilation not yet implemented";
        return result;
    }

    std::string ScriptCompiler::ExecuteCommand(const std::string& command) {
        std::string output;
        
        #ifdef _WIN32
            FILE* pipe = _popen(command.c_str(), "r");
        #else
            FILE* pipe = popen(command.c_str(), "r");
        #endif
        
        if (!pipe) {
            return "Error: Failed to execute command";
        }
        
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        
        #ifdef _WIN32
            _pclose(pipe);
        #else
            pclose(pipe);
        #endif
        
        return output;
    }

    void ScriptCompiler::ParseCompilerOutput(const std::string& output, CompileResult& result) {
        std::istringstream stream(output);
        std::string line;
        
        // Regex patterns for MSVC output
        std::regex errorPattern(".*: error [A-Z]\\d+:.*");
        std::regex warningPattern(".*: warning [A-Z]\\d+:.*");
        
        while (std::getline(stream, line)) {
            // Remove trailing whitespace
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            
            if (line.empty()) continue;
            
            // Check for errors
            if (std::regex_match(line, errorPattern) ||
                line.find("error") != std::string::npos ||
                line.find("Error") != std::string::npos ||
                line.find("ERROR") != std::string::npos) {
                
                result.errorMessages.push_back(line);
                result.errors += line + "\n";
            }
            // Check for warnings
            else if (std::regex_match(line, warningPattern) ||
                     line.find("warning") != std::string::npos ||
                     line.find("Warning") != std::string::npos) {
                
                result.warnings.push_back(line);
            }
        }
    }

    void ScriptCompiler::ReportProgress(const std::string& status, float progress) {
        if (m_ProgressCallback) {
            m_ProgressCallback(status, progress);
        }
    }

} // namespace Lunex
