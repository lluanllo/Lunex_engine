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
#include <cmath>

namespace Lunex {

    // Versión de la API de scripting
    constexpr uint32_t SCRIPTING_API_VERSION = 1;

    // Forward declarations
    class Entity;
    class Scene;

    // ========================================================================
    // MATH UTILITIES - Para usar en scripts sin dependencias externas
    // ========================================================================
    
    struct Vec3 {
        float x, y, z;
        
        Vec3() : x(0), y(0), z(0) {}
        Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
        Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}
        
        // Operadores aritméticos
        Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
        Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
        Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
        Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
        
        Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
        Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
        Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
        Vec3& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }
        
        Vec3 operator-() const { return Vec3(-x, -y, -z); }
        
        // Producto punto
        float Dot(const Vec3& other) const {
            return x * other.x + y * other.y + z * other.z;
        }
        
        // Producto cruz
        Vec3 Cross(const Vec3& other) const {
            return Vec3(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            );
        }
        
        // Magnitud
        float Length() const {
            return std::sqrt(x * x + y * y + z * z);
        }
        
        float LengthSquared() const {
            return x * x + y * y + z * z;
        }
        
        // Normalizar
        Vec3 Normalized() const {
            float len = Length();
            if (len > 0.0001f)
                return (*this) / len;
            return Vec3(0, 0, 0);
        }
        
        void Normalize() {
            float len = Length();
            if (len > 0.0001f) {
                x /= len;
                y /= len;
                z /= len;
            }
        }
        
        // Distancia
        float Distance(const Vec3& other) const {
            return (*this - other).Length();
        }
        
        // Lerp
        static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
            return a + (b - a) * t;
        }
        
        // Reflect
        Vec3 Reflect(const Vec3& normal) const {
            return (*this) - normal * 2.0f * Dot(normal);
        }
        
        // Vectores predefinidos
        static Vec3 Zero() { return Vec3(0, 0, 0); }
        static Vec3 One() { return Vec3(1, 1, 1); }
        static Vec3 Up() { return Vec3(0, 1, 0); }
        static Vec3 Down() { return Vec3(0, -1, 0); }
        static Vec3 Left() { return Vec3(-1, 0, 0); }
        static Vec3 Right() { return Vec3(1, 0, 0); }
        static Vec3 Forward() { return Vec3(0, 0, 1); }
        static Vec3 Back() { return Vec3(0, 0, -1); }
    };
    
    struct Vec2 {
        float x, y;
        
        Vec2() : x(0), y(0) {}
        Vec2(float _x, float _y) : x(_x), y(_y) {}
        Vec2(float scalar) : x(scalar), y(scalar) {}
        
        Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
        Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
        Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
        Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
        
        Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
        Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
        Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
        Vec2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }
        
        float Dot(const Vec2& other) const { return x * other.x + y * other.y; }
        
        float Length() const { return std::sqrt(x * x + y * y); }
        float LengthSquared() const { return x * x + y * y; }
        
        Vec2 Normalized() const {
            float len = Length();
            if (len > 0.0001f)
                return (*this) / len;
            return Vec2(0, 0);
        }
        
        static Vec2 Zero() { return Vec2(0, 0); }
        static Vec2 One() { return Vec2(1, 1); }
        static Vec2 Up() { return Vec2(0, 1); }
        static Vec2 Down() { return Vec2(0, -1); }
        static Vec2 Left() { return Vec2(-1, 0); }
        static Vec2 Right() { return Vec2(1, 0); }
    };
    
    // Math helpers
    namespace Math {
        constexpr float PI = 3.14159265358979323846f;
        constexpr float DEG2RAD = PI / 180.0f;
        constexpr float RAD2DEG = 180.0f / PI;
        
        inline float ToRadians(float degrees) { return degrees * DEG2RAD; }
        inline float ToDegrees(float radians) { return radians * RAD2DEG; }
        
        inline float Clamp(float value, float min, float max) {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }
        
        inline float Lerp(float a, float b, float t) {
            return a + (b - a) * t;
        }
        
        inline float Sin(float angle) { return std::sin(angle); }
        inline float Cos(float angle) { return std::cos(angle); }
        inline float Tan(float angle) { return std::tan(angle); }
        inline float Asin(float value) { return std::asin(value); }
        inline float Acos(float value) { return std::acos(value); }
        inline float Atan(float value) { return std::atan(value); }
        inline float Atan2(float y, float x) { return std::atan2(y, x); }
        
        inline float Sqrt(float value) { return std::sqrt(value); }
        inline float Pow(float base, float exp) { return std::pow(base, exp); }
        inline float Abs(float value) { return std::abs(value); }
        
        inline float Min(float a, float b) { return (a < b) ? a : b; }
        inline float Max(float a, float b) { return (a > b) ? a : b; }
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
        
        // === ENTITY HANDLE ===
        void* CurrentEntity;
        
        // Reservado para expansión futura sin romper ABI
        void* reserved[16];
    };

    // Interfaz base para módulos de script
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
