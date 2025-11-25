#pragma once

#ifdef _WIN32
    #ifdef LUNEX_SCRIPT_EXPORT
        #define LUNEX_API __declspec(dllexport)
    #else
        #define LUNEX_API __declspec(dllimport)
    #endif
#else
    #define LUNEX_API
#endif

#include <cstdint>

namespace Lunex {

    // Versión de la API de scripting
    constexpr uint32_t SCRIPTING_API_VERSION = 1;

    // Forward declarations
    class Entity;
    class Scene;

    // Estructura simple de Vector3 para evitar dependencias de GLM
    struct Vec3 {
        float x, y, z;
        Vec3() : x(0), y(0), z(0) {}
        Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    };

    // Estructura de contexto del engine que se pasa a los scripts
    struct EngineContext
    {
        // === LOGGING ===
        void (*LogInfo)(const char* message);
        void (*LogWarning)(const char* message);
        void (*LogError)(const char* message);

        // === TIME ===
        float (*GetDeltaTime)();
        float (*GetTime)();

        // === ENTITY MANAGEMENT ===
        void* (*CreateEntity)(const char* name);
        void (*DestroyEntity)(void* entity);
        
        // === TRANSFORM COMPONENT ===
        // Get/Set Position
        void (*GetEntityPosition)(void* entity, Vec3* outPosition);
        void (*SetEntityPosition)(void* entity, const Vec3* position);
        
        // Get/Set Rotation (Euler angles in radians)
        void (*GetEntityRotation)(void* entity, Vec3* outRotation);
        void (*SetEntityRotation)(void* entity, const Vec3* rotation);
        
        // Get/Set Scale
        void (*GetEntityScale)(void* entity, Vec3* outScale);
        void (*SetEntityScale)(void* entity, const Vec3* scale);
        
        // === INPUT SYSTEM ===
        // Keyboard
        bool (*IsKeyPressed)(int keyCode);
        bool (*IsKeyDown)(int keyCode);
        bool (*IsKeyReleased)(int keyCode);
        
        // Mouse
        bool (*IsMouseButtonPressed)(int button);
        bool (*IsMouseButtonDown)(int button);
        bool (*IsMouseButtonReleased)(int button);
        void (*GetMousePosition)(float* outX, float* outY);
        float (*GetMouseX)();
        float (*GetMouseY)();
        
        // === ENTITY HANDLE ===
        // Puntero a la entidad actual (el script owner)
        void* CurrentEntity;
        
        // Reservado para expansión futura sin romper ABI
        void* reserved[16];
    };

    // Interfaz base para módulos de script
    class IScriptModule
    {
    public:
        virtual ~IScriptModule() = default;

        // Llamado cuando el módulo se carga
        virtual void OnLoad(EngineContext* context) = 0;

        // Llamado cuando el módulo se descarga
        virtual void OnUnload() = 0;

        // Llamado cada frame durante el modo Play
        virtual void OnUpdate(float deltaTime) = 0;

        // Llamado para rendering (opcional)
        virtual void OnRender() {}

        // Llamado cuando el editor entra en modo Play
        virtual void OnPlayModeEnter() {}

        // Llamado cuando el editor sale del modo Play
        virtual void OnPlayModeExit() {}
    };

} // namespace Lunex

// Funciones que deben exportar los plugins
extern "C"
{
    // Obtener versión de la API del plugin
    LUNEX_API uint32_t Lunex_GetScriptingAPIVersion();

    // Factory para crear instancia del módulo
    LUNEX_API Lunex::IScriptModule* Lunex_CreateModule();

    // Destructor del módulo
    LUNEX_API void Lunex_DestroyModule(Lunex::IScriptModule* module);
}
