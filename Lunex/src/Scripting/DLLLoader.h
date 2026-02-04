#pragma once

/**
 * @file DLLLoader.h
 * @brief AAA Architecture: Cross-platform dynamic library loader
 * 
 * Provides safe loading/unloading of DLLs with error handling
 * and function pointer retrieval.
 */

#include <string>
#include <functional>

#ifdef _WIN32
    #include <Windows.h>
    using ModuleHandle = HMODULE;
#else
    #include <dlfcn.h>
    using ModuleHandle = void*;
#endif

namespace Lunex {

    /**
     * @class DLLLoader
     * @brief Cross-platform DLL/SO loader with RAII
     */
    class DLLLoader {
    public:
        DLLLoader() = default;
        
        DLLLoader(const std::string& path) {
            Load(path);
        }
        
        ~DLLLoader() {
            Unload();
        }

        // Non-copyable
        DLLLoader(const DLLLoader&) = delete;
        DLLLoader& operator=(const DLLLoader&) = delete;

        // Movable
        DLLLoader(DLLLoader&& other) noexcept
            : m_Handle(other.m_Handle)
            , m_LoadedPath(std::move(other.m_LoadedPath))
            , m_LastError(std::move(other.m_LastError))
        {
            other.m_Handle = nullptr;
        }

        DLLLoader& operator=(DLLLoader&& other) noexcept {
            if (this != &other) {
                Unload();
                m_Handle = other.m_Handle;
                m_LoadedPath = std::move(other.m_LoadedPath);
                m_LastError = std::move(other.m_LastError);
                other.m_Handle = nullptr;
            }
            return *this;
        }

        /**
         * @brief Load a DLL/SO from the specified path
         * @param path Full path to the library
         * @return True if loaded successfully
         */
        bool Load(const std::string& path) {
            if (m_Handle) {
                Unload();
            }

            m_LastError.clear();

            #ifdef _WIN32
                m_Handle = LoadLibraryA(path.c_str());
                if (!m_Handle) {
                    m_LastError = GetWindowsError();
                    return false;
                }
            #else
                m_Handle = dlopen(path.c_str(), RTLD_NOW);
                if (!m_Handle) {
                    const char* error = dlerror();
                    m_LastError = error ? error : "Unknown error";
                    return false;
                }
            #endif

            m_LoadedPath = path;
            return true;
        }

        /**
         * @brief Unload the currently loaded library
         */
        void Unload() {
            if (!m_Handle) return;

            #ifdef _WIN32
                FreeLibrary(m_Handle);
            #else
                dlclose(m_Handle);
            #endif

            m_Handle = nullptr;
            m_LoadedPath.clear();
        }

        /**
         * @brief Get a function pointer from the loaded library
         * @tparam FuncPtr Function pointer type
         * @param name Function name
         * @return Function pointer or nullptr if not found
         */
        template<typename FuncPtr>
        FuncPtr GetFunction(const std::string& name) {
            if (!m_Handle) {
                m_LastError = "Library not loaded";
                return nullptr;
            }

            #ifdef _WIN32
                void* func = reinterpret_cast<void*>(GetProcAddress(m_Handle, name.c_str()));
            #else
                void* func = dlsym(m_Handle, name.c_str());
            #endif

            if (!func) {
                #ifdef _WIN32
                    m_LastError = "Function not found: " + name + " - " + GetWindowsError();
                #else
                    const char* error = dlerror();
                    m_LastError = "Function not found: " + name + " - " + (error ? error : "Unknown error");
                #endif
                return nullptr;
            }

            return reinterpret_cast<FuncPtr>(func);
        }

        /**
         * @brief Check if a library is loaded
         */
        bool IsLoaded() const { return m_Handle != nullptr; }

        /**
         * @brief Get the path of the loaded library
         */
        const std::string& GetLoadedPath() const { return m_LoadedPath; }

        /**
         * @brief Get the last error message
         */
        const std::string& GetLastError() const { return m_LastError; }

        /**
         * @brief Get the native handle
         */
        ModuleHandle GetNativeHandle() const { return m_Handle; }

    private:
        ModuleHandle m_Handle = nullptr;
        std::string m_LoadedPath;
        std::string m_LastError;

        #ifdef _WIN32
        std::string GetWindowsError() {
            DWORD error = ::GetLastError();
            if (error == 0) return "";

            LPSTR buffer = nullptr;
            size_t size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&buffer,
                0,
                nullptr
            );

            std::string message(buffer, size);
            LocalFree(buffer);

            // Remove trailing newline
            while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
                message.pop_back();
            }

            return message + " (Error: " + std::to_string(error) + ")";
        }
        #endif
    };

    /**
     * @class DLLGuard
     * @brief RAII guard that unloads a DLL when destroyed
     */
    class DLLGuard {
    public:
        explicit DLLGuard(DLLLoader& loader) : m_Loader(loader) {}
        
        ~DLLGuard() {
            m_Loader.Unload();
        }

        DLLGuard(const DLLGuard&) = delete;
        DLLGuard& operator=(const DLLGuard&) = delete;

    private:
        DLLLoader& m_Loader;
    };

} // namespace Lunex
