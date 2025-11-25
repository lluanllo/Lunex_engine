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
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace Lunex {

    // Versión de la API de scripting
    constexpr uint32_t SCRIPTING_API_VERSION = 1;

    // Forward declarations
    class Entity;
    class Scene;

    // ========================================================================
    // MATH UTILITIES - Thin wrappers around GLM for script API
    // ========================================================================
    
    // Lightweight Vec3 wrapper that can convert to/from glm::vec3
    struct Vec3 {
        float x, y, z;
        
        Vec3() : x(0), y(0), z(0) {}
        Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
        Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}
        
        // GLM interop
        Vec3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}
        operator glm::vec3() const { return glm::vec3(x, y, z); }
        
        // Basic operators
        Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
        Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
        Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
        Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
        
        Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
        Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
        Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
        Vec3& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }
        
        Vec3 operator-() const { return Vec3(-x, -y, -z); }
        
        // Common vector operations (inline, using GLM)
        float Dot(const Vec3& other) const {
            return glm::dot(glm::vec3(*this), glm::vec3(other));
        }
        
        Vec3 Cross(const Vec3& other) const {
            glm::vec3 result = glm::cross(glm::vec3(*this), glm::vec3(other));
            return Vec3(result);
        }
        
        float Length() const {
            return glm::length(glm::vec3(*this));
        }
        
        float LengthSquared() const { 
            return x * x + y * y + z * z; 
        }
        
        Vec3 Normalized() const {
            glm::vec3 result = glm::normalize(glm::vec3(*this));
            return Vec3(result);
        }
        
        void Normalize() {
            glm::vec3 v = glm::normalize(glm::vec3(*this));
            x = v.x;
            y = v.y;
            z = v.z;
        }
        
        float Distance(const Vec3& other) const {
            return glm::distance(glm::vec3(*this), glm::vec3(other));
        }
        
        // Static helpers
        static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
            glm::vec3 result = glm::mix(glm::vec3(a), glm::vec3(b), t);
            return Vec3(result);
        }
        
        Vec3 Reflect(const Vec3& normal) const {
            glm::vec3 result = glm::reflect(glm::vec3(*this), glm::vec3(normal));
            return Vec3(result);
        }
        
        // Predefined vectors
        static Vec3 Zero() { return Vec3(0, 0, 0); }
        static Vec3 One() { return Vec3(1, 1, 1); }
        static Vec3 Up() { return Vec3(0, 1, 0); }
        static Vec3 Down() { return Vec3(0, -1, 0); }
        static Vec3 Left() { return Vec3(-1, 0, 0); }
        static Vec3 Right() { return Vec3(1, 0, 0); }
        static Vec3 Forward() { return Vec3(0, 0, 1); }
        static Vec3 Back() { return Vec3(0, 0, -1); }
    };
    
    // Lightweight Vec2 wrapper
    struct Vec2 {
        float x, y;
        
        Vec2() : x(0), y(0) {}
        Vec2(float _x, float _y) : x(_x), y(_y) {}
        Vec2(float scalar) : x(scalar), y(scalar) {}
        
        // GLM interop
        Vec2(const glm::vec2& v) : x(v.x), y(v.y) {}
        operator glm::vec2() const { return glm::vec2(x, y); }
        
        Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
        Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
        Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
        Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
        
        Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
        Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
        Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
        Vec2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }
        
        float Dot(const Vec2& other) const {
            return glm::dot(glm::vec2(*this), glm::vec2(other));
        }
        
        float Length() const {
            return glm::length(glm::vec2(*this));
        }
        
        float LengthSquared() const { 
            return x * x + y * y; 
        }
        
        Vec2 Normalized() const {
            glm::vec2 result = glm::normalize(glm::vec2(*this));
            return Vec2(result);
        }
        
        static Vec2 Zero() { return Vec2(0, 0); }
        static Vec2 One() { return Vec2(1, 1); }
        static Vec2 Up() { return Vec2(0, 1); }
        static Vec2 Down() { return Vec2(0, -1); }
        static Vec2 Left() { return Vec2(-1, 0); }
        static Vec2 Right() { return Vec2(1, 0); }
    };
    
    // Math helpers - Thin wrappers around GLM functions (inline)
    namespace Math {
        constexpr float PI = 3.14159265358979323846f;
        constexpr float DEG2RAD = PI / 180.0f;
        constexpr float RAD2DEG = 180.0f / PI;
        
        inline float ToRadians(float degrees) { return glm::radians(degrees); }
        inline float ToDegrees(float radians) { return glm::degrees(radians); }
        inline float Clamp(float value, float min, float max) { return glm::clamp(value, min, max); }
        inline float Lerp(float a, float b, float t) { return glm::mix(a, b, t); }
        
        inline float Sin(float angle) { return glm::sin(angle); }
        inline float Cos(float angle) { return glm::cos(angle); }
        inline float Tan(float angle) { return glm::tan(angle); }
        inline float Asin(float value) { return glm::asin(value); }
        inline float Acos(float value) { return glm::acos(value); }
        inline float Atan(float value) { return glm::atan(value); }
        inline float Atan2(float y, float x) { return glm::atan(y, x); }
        
        inline float Sqrt(float value) { return glm::sqrt(value); }
        inline float Pow(float base, float exp) { return glm::pow(base, exp); }
        inline float Abs(float value) { return glm::abs(value); }
        inline float Min(float a, float b) { return glm::min(a, b); }
        inline float Max(float a, float b) { return glm::max(a, b); }
    }

    // ========================================================================
    // SCRIPTING API
    // ========================================================================

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
        void (*GetEntityPosition)(void* entity, Vec3* outPosition);
        void (*SetEntityPosition)(void* entity, const Vec3* position);
        
        void (*GetEntityRotation)(void* entity, Vec3* outRotation);
        void (*SetEntityRotation)(void* entity, const Vec3* rotation);
        
        void (*GetEntityScale)(void* entity, Vec3* outScale);
        void (*SetEntityScale)(void* entity, const Vec3* scale);
        
        // === INPUT SYSTEM ===
        bool (*IsKeyPressed)(int keyCode);
        bool (*IsKeyDown)(int keyCode);
        bool (*IsKeyReleased)(int keyCode);
        
        bool (*IsMouseButtonPressed)(int button);
        bool (*IsMouseButtonDown)(int button);
        bool (*IsMouseButtonReleased)(int button);
        void (*GetMousePosition)(float* outX, float* outY);
        float (*GetMouseX)();
        float (*GetMouseY)();
        
        // === RIGIDBODY2D COMPONENT ===
        bool (*HasRigidbody2D)(void* entity);
        void (*GetLinearVelocity)(void* entity, Vec2* outVelocity);
        void (*SetLinearVelocity)(void* entity, const Vec2* velocity);
        void (*ApplyLinearImpulse)(void* entity, const Vec2* impulse, bool wake);
        void (*ApplyLinearImpulseToCenter)(void* entity, const Vec2* impulse, bool wake);
        void (*ApplyForce)(void* entity, const Vec2* force, const Vec2* point, bool wake);
        void (*ApplyForceToCenter)(void* entity, const Vec2* force, bool wake);
        float (*GetMass)(void* entity);
        float (*GetGravityScale)(void* entity);
        void (*SetGravityScale)(void* entity, float scale);
        
        // === ENTITY HANDLE ===
        void* CurrentEntity;
        
        // Reservado para expansion futura sin romper ABI
        void* reserved[16];
    };

    // Interfaz base para modulos de script
    class IScriptModule
    {
    public:
        virtual ~IScriptModule() = default;

        virtual void OnLoad(EngineContext* context) = 0;
        virtual void OnUnload() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnRender() {}
        virtual void OnPlayModeEnter() {}
        virtual void OnPlayModeExit() {}
    };

} // namespace Lunex

// Funciones que deben exportar los plugins
extern "C"
{
    LUNEX_API uint32_t Lunex_GetScriptingAPIVersion();
    LUNEX_API Lunex::IScriptModule* Lunex_CreateModule();
    LUNEX_API void Lunex_DestroyModule(Lunex::IScriptModule* module);
}
