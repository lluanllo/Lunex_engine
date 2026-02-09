#pragma once

/**
 * @file ScriptCompiler.h
 * @brief AAA Architecture: Automated script compilation system
 *
 * Compiles C++ scripts into DLLs using the system compiler.
 * Supports Visual Studio on Windows and GCC/Clang on Unix.
 */

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <functional>

namespace Lunex {

    /**
     * @struct CompileOptions
     * @brief Configuration for script compilation
     */
    struct CompileOptions {
        std::string sourceDir = "Lunex-ScriptCore";
        std::string outputDir = "bin/Scripts";
        std::string configuration = "Debug";   // "Debug" or "Release"
        bool verbose = true;
        bool includeDebugSymbols = true;

        // Additional include paths
        std::vector<std::string> includePaths;

        // Additional library paths
        std::vector<std::string> libraryPaths;

        // Additional libraries to link
        std::vector<std::string> libraries;

        // Preprocessor defines
        std::vector<std::string> defines;
    };

    /**
     * @struct CompileResult
     * @brief Result of a compilation attempt
     */
    struct CompileResult {
        bool success = false;
        std::string outputPath;          // Path to output DLL
        std::string output;              // Compiler stdout
        std::string errors;              // Compiler stderr
        double compileTimeMs = 0.0;

        // Warnings and errors parsed from output
        std::vector<std::string> warnings;
        std::vector<std::string> errorMessages;
    };

    /**
     * @class ScriptCompiler
     * @brief Compiles C++ script files into loadable DLLs
     */
    class ScriptCompiler {
    public:
        using ProgressCallback = std::function<void(const std::string& status, float progress)>;

        /**
         * @brief Set progress callback for compilation status updates
         */
        void SetProgressCallback(ProgressCallback callback) {
            m_ProgressCallback = callback;
        }

        /**
         * @brief Compile a single script file
         * @param scriptPath Path to .cpp file
         * @param options Compilation options
         * @return Compilation result
         */
        CompileResult Compile(const std::string& scriptPath, const CompileOptions& options = {});

        /**
         * @brief Compile multiple script files in parallel
         * @param scriptPaths List of .cpp files
         * @param options Compilation options
         * @return List of compilation results
         */
        std::vector<CompileResult> CompileBatch(
            const std::vector<std::string>& scriptPaths,
            const CompileOptions& options = {}
        );

        /**
         * @brief Check if a script needs recompilation
         * @param scriptPath Source script path
         * @param dllPath Compiled DLL path
         * @return True if DLL is out of date
         */
        bool NeedsRecompile(const std::string& scriptPath, const std::string& dllPath);

        /**
         * @brief Get the expected DLL output path for a script
         */
        std::string GetDLLPath(const std::string& scriptPath, const CompileOptions& options = {});

        /**
         * @brief Detect Visual Studio installation
         * @return True if VS is installed and can be used
         */
        bool DetectVisualStudio();

        /**
         * @brief Get detected Visual Studio path
         */
        const std::string& GetVisualStudioPath() const { return m_VSPath; }

        /**
         * @brief Get vcvars64.bat path
         */
        const std::string& GetVCVarsPath() const { return m_VCVarsPath; }

        /**
         * @brief Get cl.exe path
         */
        const std::string& GetCLPath() const { return m_CLPath; }

    private:
        CompileResult CompileWindows(const std::string& scriptPath, const CompileOptions& options);
        CompileResult CompileUnix(const std::string& scriptPath, const CompileOptions& options);

        std::string ExecuteCommand(const std::string& command);
        void ParseCompilerOutput(const std::string& output, CompileResult& result);
        void ReportProgress(const std::string& status, float progress);

        std::string m_VSPath;
        std::string m_VCVarsPath;
        std::string m_CLPath;
        bool m_VSDetected = false;

        ProgressCallback m_ProgressCallback;
    };

} // namespace Lunex
