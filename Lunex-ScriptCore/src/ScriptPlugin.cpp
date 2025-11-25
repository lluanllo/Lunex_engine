#include "ScriptPlugin.h"
#include <iostream>

namespace Lunex {

    ScriptPlugin::~ScriptPlugin()
    {
        Unload();
    }

    ScriptPlugin::ScriptPlugin(ScriptPlugin&& other) noexcept
        : m_Handle(other.m_Handle)
        , m_Module(other.m_Module)
        , m_Path(std::move(other.m_Path))
        , m_APIVersion(other.m_APIVersion)
        , m_GetAPIVersionFn(other.m_GetAPIVersionFn)
        , m_CreateModuleFn(other.m_CreateModuleFn)
        , m_DestroyModuleFn(other.m_DestroyModuleFn)
    {
        other.m_Handle = nullptr;
        other.m_Module = nullptr;
        other.m_APIVersion = 0;
        other.m_GetAPIVersionFn = nullptr;
        other.m_CreateModuleFn = nullptr;
        other.m_DestroyModuleFn = nullptr;
    }

    ScriptPlugin& ScriptPlugin::operator=(ScriptPlugin&& other) noexcept
    {
        if (this != &other)
        {
            Unload();

            m_Handle = other.m_Handle;
            m_Module = other.m_Module;
            m_Path = std::move(other.m_Path);
            m_APIVersion = other.m_APIVersion;
            m_GetAPIVersionFn = other.m_GetAPIVersionFn;
            m_CreateModuleFn = other.m_CreateModuleFn;
            m_DestroyModuleFn = other.m_DestroyModuleFn;

            other.m_Handle = nullptr;
            other.m_Module = nullptr;
            other.m_APIVersion = 0;
            other.m_GetAPIVersionFn = nullptr;
            other.m_CreateModuleFn = nullptr;
            other.m_DestroyModuleFn = nullptr;
        }
        return *this;
    }

    bool ScriptPlugin::Load(const std::string& path, EngineContext* context)
    {
        if (IsLoaded())
        {
            std::cerr << "[ScriptPlugin] Ya hay un plugin cargado. Descargándolo primero.\n";
            Unload();
        }

        // Cargar la librería dinámica
#ifdef _WIN32
        m_Handle = LoadLibraryA(path.c_str());
        if (!m_Handle)
        {
            DWORD error = GetLastError();
            std::cerr << "[ScriptPlugin] Error al cargar " << path << " - Código de error: " << error << "\n";
            return false;
        }
#else
        m_Handle = dlopen(path.c_str(), RTLD_NOW);
        if (!m_Handle)
        {
            std::cerr << "[ScriptPlugin] Error al cargar " << path << " - " << dlerror() << "\n";
            return false;
        }
#endif

        // Obtener punteros a las funciones exportadas
        m_GetAPIVersionFn = (GetAPIVersionFn)GetFunctionPointer("Lunex_GetScriptingAPIVersion");
        m_CreateModuleFn = (CreateModuleFn)GetFunctionPointer("Lunex_CreateModule");
        m_DestroyModuleFn = (DestroyModuleFn)GetFunctionPointer("Lunex_DestroyModule");

        if (!m_GetAPIVersionFn || !m_CreateModuleFn || !m_DestroyModuleFn)
        {
            std::cerr << "[ScriptPlugin] El plugin no exporta las funciones requeridas.\n";
            Unload();
            return false;
        }

        // Verificar versión de la API
        m_APIVersion = m_GetAPIVersionFn();
        if (m_APIVersion != SCRIPTING_API_VERSION)
        {
            std::cerr << "[ScriptPlugin] Incompatibilidad de versión de API. "
                      << "Engine: " << SCRIPTING_API_VERSION 
                      << ", Plugin: " << m_APIVersion << "\n";
            Unload();
            return false;
        }

        // Crear instancia del módulo
        m_Module = m_CreateModuleFn();
        if (!m_Module)
        {
            std::cerr << "[ScriptPlugin] Error al crear instancia del módulo.\n";
            Unload();
            return false;
        }

        m_Path = path;

        // Llamar a OnLoad
        m_Module->OnLoad(context);

        std::cout << "[ScriptPlugin] Plugin cargado exitosamente: " << path << "\n";
        return true;
    }

    void ScriptPlugin::Unload()
    {
        if (m_Module)
        {
            m_Module->OnUnload();
            
            if (m_DestroyModuleFn)
            {
                m_DestroyModuleFn(m_Module);
            }
            m_Module = nullptr;
        }

        if (m_Handle)
        {
#ifdef _WIN32
            FreeLibrary(m_Handle);
#else
            dlclose(m_Handle);
#endif
            m_Handle = nullptr;
        }

        Reset();
    }

    void ScriptPlugin::Update(float deltaTime)
    {
        if (m_Module)
        {
            m_Module->OnUpdate(deltaTime);
        }
    }

    void ScriptPlugin::Render()
    {
        if (m_Module)
        {
            m_Module->OnRender();
        }
    }

    void ScriptPlugin::OnPlayModeEnter()
    {
        if (m_Module)
        {
            m_Module->OnPlayModeEnter();
        }
    }

    void ScriptPlugin::OnPlayModeExit()
    {
        if (m_Module)
        {
            m_Module->OnPlayModeExit();
        }
    }

    void* ScriptPlugin::GetFunctionPointer(const char* name)
    {
        if (!m_Handle)
            return nullptr;

#ifdef _WIN32
        return (void*)GetProcAddress(m_Handle, name);
#else
        return dlsym(m_Handle, name);
#endif
    }

    void ScriptPlugin::Reset()
    {
        m_Path.clear();
        m_APIVersion = 0;
        m_GetAPIVersionFn = nullptr;
        m_CreateModuleFn = nullptr;
        m_DestroyModuleFn = nullptr;
    }

} // namespace Lunex
