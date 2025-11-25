#pragma once

#include "LunexScriptingAPI.h"
#include <string>
#include <memory>

#ifdef _WIN32
    #include <windows.h>
    using LibHandle = HMODULE;
#else
    #include <dlfcn.h>
    using LibHandle = void*;
#endif

namespace Lunex {

    // Alias para las funciones exportadas por los plugins
    using GetAPIVersionFn = uint32_t(*)();
    using CreateModuleFn = IScriptModule*(*)();
    using DestroyModuleFn = void(*)(IScriptModule*);

    class ScriptPlugin
    {
    public:
        ScriptPlugin() = default;
        ~ScriptPlugin();

        // No copiable
        ScriptPlugin(const ScriptPlugin&) = delete;
        ScriptPlugin& operator=(const ScriptPlugin&) = delete;

        // Movible
        ScriptPlugin(ScriptPlugin&& other) noexcept;
        ScriptPlugin& operator=(ScriptPlugin&& other) noexcept;

        // Carga un plugin desde la ruta especificada
        bool Load(const std::string& path, EngineContext* context);

        // Descarga el plugin actual
        void Unload();

        // Verifica si el plugin está cargado
        bool IsLoaded() const { return m_Module != nullptr; }

        // Obtiene la ruta del plugin cargado
        const std::string& GetPath() const { return m_Path; }

        // Obtiene la versión de la API del plugin
        uint32_t GetAPIVersion() const { return m_APIVersion; }

        // Llama a OnUpdate del módulo
        void Update(float deltaTime);

        // Llama a OnRender del módulo
        void Render();

        // Control de modo Play
        void OnPlayModeEnter();
        void OnPlayModeExit();

    private:
        void* GetFunctionPointer(const char* name);
        void Reset();

    private:
        LibHandle m_Handle = nullptr;
        IScriptModule* m_Module = nullptr;
        std::string m_Path;
        uint32_t m_APIVersion = 0;

        GetAPIVersionFn m_GetAPIVersionFn = nullptr;
        CreateModuleFn m_CreateModuleFn = nullptr;
        DestroyModuleFn m_DestroyModuleFn = nullptr;
    };

} // namespace Lunex
