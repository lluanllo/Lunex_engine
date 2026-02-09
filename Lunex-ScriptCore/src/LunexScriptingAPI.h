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
#include <string>
#include <vector>
#include <functional>

namespace Lunex {

    // ========================================================================
    // AAA SCRIPTING API VERSION
    // ========================================================================

    // Versi�n de la API de scripting
    // Increment when making breaking changes to EngineContext
    constexpr uint32_t SCRIPTING_API_VERSION = 3;

    // Forward declarations
    class Entity;
    class Scene;

    // ========================================================================
    // KEY CODES - Input System
    // ========================================================================

    // Key codes for input (matching GLFW/engine key codes)
#define LNX_KEY_SPACE           32
#define LNX_KEY_APOSTROPHE      39  /* ' */
#define LNX_KEY_COMMA           44  /* , */
#define LNX_KEY_MINUS           45  /* - */
#define LNX_KEY_PERIOD          46  /* . */
#define LNX_KEY_SLASH           47  /* / */
#define LNX_KEY_0               48
#define LNX_KEY_1               49
#define LNX_KEY_2               50
#define LNX_KEY_3               51
#define LNX_KEY_4               52
#define LNX_KEY_5               53
#define LNX_KEY_6               54
#define LNX_KEY_7               55
#define LNX_KEY_8               56
#define LNX_KEY_9               57
#define LNX_KEY_SEMICOLON       59  /* ; */
#define LNX_KEY_EQUAL           61  /* = */
#define LNX_KEY_A               65
#define LNX_KEY_B               66
#define LNX_KEY_C               67
#define LNX_KEY_D               68
#define LNX_KEY_E               69
#define LNX_KEY_F               70
#define LNX_KEY_G               71
#define LNX_KEY_H               72
#define LNX_KEY_I               73
#define LNX_KEY_J               74
#define LNX_KEY_K               75
#define LNX_KEY_L               76
#define LNX_KEY_M               77
#define LNX_KEY_N               78
#define LNX_KEY_O               79
#define LNX_KEY_P               80
#define LNX_KEY_Q               81
#define LNX_KEY_R               82
#define LNX_KEY_S               83
#define LNX_KEY_T               84
#define LNX_KEY_U               85
#define LNX_KEY_V               86
#define LNX_KEY_W               87
#define LNX_KEY_X               88
#define LNX_KEY_Y               89
#define LNX_KEY_Z               90
#define LNX_KEY_LEFT_BRACKET    91  /* [ */
#define LNX_KEY_BACKSLASH       92  /* \ */
#define LNX_KEY_RIGHT_BRACKET   93  /* ] */
#define LNX_KEY_GRAVE_ACCENT    96  /* ` */

// Function keys
#define LNX_KEY_ESCAPE          256
#define LNX_KEY_ENTER           257
#define LNX_KEY_TAB             258
#define LNX_KEY_BACKSPACE       259
#define LNX_KEY_INSERT          260
#define LNX_KEY_DELETE          261
#define LNX_KEY_RIGHT           262
#define LNX_KEY_LEFT            263
#define LNX_KEY_DOWN            264
#define LNX_KEY_UP              265
#define LNX_KEY_PAGE_UP         266
#define LNX_KEY_PAGE_DOWN       267
#define LNX_KEY_HOME            268
#define LNX_KEY_END             269
#define LNX_KEY_CAPS_LOCK       280
#define LNX_KEY_SCROLL_LOCK     281
#define LNX_KEY_NUM_LOCK        282
#define LNX_KEY_PRINT_SCREEN    283
#define LNX_KEY_PAUSE           284
#define LNX_KEY_F1              290
#define LNX_KEY_F2              291
#define LNX_KEY_F3              292
#define LNX_KEY_F4              293
#define LNX_KEY_F5              294
#define LNX_KEY_F6              295
#define LNX_KEY_F7              296
#define LNX_KEY_F8              297
#define LNX_KEY_F9              298
#define LNX_KEY_F10             299
#define LNX_KEY_F11             300
#define LNX_KEY_F12             301

// Keypad
#define LNX_KEY_KP_0            320
#define LNX_KEY_KP_1            321
#define LNX_KEY_KP_2            322
#define LNX_KEY_KP_3            323
#define LNX_KEY_KP_4            324
#define LNX_KEY_KP_5            325
#define LNX_KEY_KP_6            326
#define LNX_KEY_KP_7            327
#define LNX_KEY_KP_8            328
#define LNX_KEY_KP_9            329
#define LNX_KEY_KP_DECIMAL      330
#define LNX_KEY_KP_DIVIDE       331
#define LNX_KEY_KP_MULTIPLY     332
#define LNX_KEY_KP_SUBTRACT     333
#define LNX_KEY_KP_ADD          334
#define LNX_KEY_KP_ENTER        335
#define LNX_KEY_KP_EQUAL        336

// Modifiers
#define LNX_KEY_LEFT_SHIFT      340
#define LNX_KEY_LEFT_CONTROL    341
#define LNX_KEY_LEFT_ALT        342
#define LNX_KEY_LEFT_SUPER      343
#define LNX_KEY_RIGHT_SHIFT     344
#define LNX_KEY_RIGHT_CONTROL   345
#define LNX_KEY_RIGHT_ALT       346
#define LNX_KEY_RIGHT_SUPER     347
#define LNX_KEY_MENU            348

// Mouse button codes
#define LNX_MOUSE_BUTTON_1      0
#define LNX_MOUSE_BUTTON_2      1
#define LNX_MOUSE_BUTTON_3      2
#define LNX_MOUSE_BUTTON_4      3
#define LNX_MOUSE_BUTTON_5      4
#define LNX_MOUSE_BUTTON_6      5
#define LNX_MOUSE_BUTTON_7      6
#define LNX_MOUSE_BUTTON_8      7
#define LNX_MOUSE_BUTTON_LEFT   LNX_MOUSE_BUTTON_1
#define LNX_MOUSE_BUTTON_RIGHT  LNX_MOUSE_BUTTON_2
#define LNX_MOUSE_BUTTON_MIDDLE LNX_MOUSE_BUTTON_3

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
    // REFLECTION SYSTEM - Public Variable Exposure
    // ========================================================================

    enum class VarType {
        Float,
        Int,
        Bool,
        Vec2,
        Vec3,
        String
    };

    struct VarMetadata {
        std::string Name;
        VarType Type;
        void* DataPtr;

        // Range constraints (optional)
        bool HasRange = false;
        float MinValue = 0.0f;
        float MaxValue = 0.0f;

        // UI hints
        std::string Tooltip;

        VarMetadata(const std::string& name, VarType type, void* ptr)
            : Name(name), Type(type), DataPtr(ptr) {
        }
    };

    // ========================================================================
    // HELPER WRAPPERS - Simplified Script API
    // ========================================================================

    // Forward declaration
    struct EngineContext;

    class TransformAPI {
    public:
        TransformAPI() : m_Context(nullptr), m_Entity(nullptr) {}
        TransformAPI(EngineContext* ctx, void* entity) : m_Context(ctx), m_Entity(entity) {}

        Vec3 GetPosition();
        void SetPosition(const Vec3& pos);
        void Translate(const Vec3& delta);

        Vec3 GetRotation();
        void SetRotation(const Vec3& rot);
        void Rotate(const Vec3& delta);

        Vec3 GetScale();
        void SetScale(const Vec3& scale);

    private:
        EngineContext* m_Context;
        void* m_Entity;
    };

    class Rigidbody2DAPI {
    public:
        Rigidbody2DAPI() : m_Context(nullptr), m_Entity(nullptr) {}
        Rigidbody2DAPI(EngineContext* ctx, void* entity) : m_Context(ctx), m_Entity(entity) {}

        bool Exists();

        Vec2 GetVelocity();
        void SetVelocity(const Vec2& vel);
        void AddVelocity(const Vec2& delta);

        void ApplyImpulse(const Vec2& impulse);
        void ApplyForce(const Vec2& force);

        float GetMass();
        float GetGravityScale();
        void SetGravityScale(float scale);

    private:
        EngineContext* m_Context;
        void* m_Entity;
    };

    // NEW: Rigidbody3D API for Bullet3 physics
    class Rigidbody3DAPI {
    public:
        Rigidbody3DAPI() : m_Context(nullptr), m_Entity(nullptr) {}
        Rigidbody3DAPI(EngineContext* ctx, void* entity) : m_Context(ctx), m_Entity(entity) {}

        bool Exists();

        // Velocity
        Vec3 GetLinearVelocity();
        void SetLinearVelocity(const Vec3& vel);
        void AddLinearVelocity(const Vec3& delta);

        Vec3 GetAngularVelocity();
        void SetAngularVelocity(const Vec3& vel);

        // Forces
        void ApplyForce(const Vec3& force);
        void ApplyForceAtPoint(const Vec3& force, const Vec3& point);
        void ApplyImpulse(const Vec3& impulse);
        void ApplyImpulseAtPoint(const Vec3& impulse, const Vec3& point);
        void ApplyTorque(const Vec3& torque);
        void ApplyTorqueImpulse(const Vec3& torque);

        // Properties
        float GetMass();
        void SetMass(float mass);

        float GetFriction();
        void SetFriction(float friction);

        float GetRestitution();
        void SetRestitution(float restitution);

        float GetLinearDamping();
        void SetLinearDamping(float damping);

        float GetAngularDamping();
        void SetAngularDamping(float damping);

        // Constraints
        void SetLinearFactor(const Vec3& factor);
        Vec3 GetLinearFactor();

        void SetAngularFactor(const Vec3& factor);
        Vec3 GetAngularFactor();

        // State
        void SetKinematic(bool kinematic);
        bool IsKinematic();

        void ClearForces();
        void Activate();
        void Deactivate();
        bool IsActive();

    private:
        EngineContext* m_Context;
        void* m_Entity;
    };

    class InputAPI {
    public:
        InputAPI() : m_Context(nullptr) {}
        InputAPI(EngineContext* ctx) : m_Context(ctx) {}

        bool IsKeyPressed(int key);
        bool IsKeyDown(int key);
        bool IsKeyReleased(int key);

        bool IsMousePressed(int button);
        Vec2 GetMousePosition();

    private:
        EngineContext* m_Context;
    };

    class TimeAPI {
    public:
        TimeAPI() : m_Context(nullptr) {}
        TimeAPI(EngineContext* ctx) : m_Context(ctx) {}

        float DeltaTime();
        float GetTime();

    private:
        EngineContext* m_Context;
    };

    class DebugAPI {
    public:
        DebugAPI() : m_Context(nullptr) {}
        DebugAPI(EngineContext* ctx) : m_Context(ctx) {}

        void Log(const std::string& message);
        void LogWarning(const std::string& message);
        void LogError(const std::string& message);

    private:
        EngineContext* m_Context;
    };

    // ========================================================================
    // SCRIPTING API CONTEXT
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

        // === RIGIDBODY3D COMPONENT (Bullet3) ===
        bool (*HasRigidbody3D)(void* entity);
        void (*GetLinearVelocity3D)(void* entity, Vec3* outVelocity);
        void (*SetLinearVelocity3D)(void* entity, const Vec3* velocity);
        void (*GetAngularVelocity3D)(void* entity, Vec3* outVelocity);
        void (*SetAngularVelocity3D)(void* entity, const Vec3* velocity);

        void (*ApplyForce3D)(void* entity, const Vec3* force);
        void (*ApplyForceAtPoint3D)(void* entity, const Vec3* force, const Vec3* point);
        void (*ApplyImpulse3D)(void* entity, const Vec3* impulse);
        void (*ApplyImpulseAtPoint3D)(void* entity, const Vec3* impulse, const Vec3* point);
        void (*ApplyTorque3D)(void* entity, const Vec3* torque);
        void (*ApplyTorqueImpulse3D)(void* entity, const Vec3* torque);

        float (*GetMass3D)(void* entity);
        void (*SetMass3D)(void* entity, float mass);
        float (*GetFriction3D)(void* entity);
        void (*SetFriction3D)(void* entity, float friction);
        float (*GetRestitution3D)(void* entity);
        void (*SetRestitution3D)(void* entity, float restitution);
        float (*GetLinearDamping3D)(void* entity);
        void (*SetLinearDamping3D)(void* entity, float damping);
        float (*GetAngularDamping3D)(void* entity);
        void (*SetAngularDamping3D)(void* entity, float damping);

        void (*SetLinearFactor3D)(void* entity, const Vec3* factor);
        void (*GetLinearFactor3D)(void* entity, Vec3* outFactor);
        void (*SetAngularFactor3D)(void* entity, const Vec3* factor);
        void (*GetAngularFactor3D)(void* entity, Vec3* outFactor);

        void (*ClearForces3D)(void* entity);
        void (*Activate3D)(void* entity);
        void (*Deactivate3D)(void* entity);
        bool (*IsActive3D)(void* entity);
        bool (*IsKinematic3D)(void* entity);
        void (*SetKinematic3D)(void* entity, bool kinematic);

        // === ENTITY HANDLE ===
        void* CurrentEntity;

        // Reservado para expansion futura sin romper ABI
        void* reserved[16];
    };

    // ========================================================================
    // BASE SCRIPT CLASS - Simplified Interface
    // ========================================================================

    class IScriptModule
    {
    public:
        virtual ~IScriptModule() = default;

        // Engine callbacks (internal use)
        virtual void OnLoad(EngineContext* context) = 0;
        virtual void OnUnload() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnRender() {}
        virtual void OnPlayModeEnter() {}
        virtual void OnPlayModeExit() {}

        // Reflection system
        virtual std::vector<VarMetadata> GetPublicVariables() { return {}; }
    };

    // ========================================================================
    // SIMPLIFIED SCRIPT BASE CLASS - Inherit from this!
    // ========================================================================

    class Script : public IScriptModule {
    public:
        virtual ~Script() = default;

        // Internal engine callbacks (don't override these)
        void OnLoad(EngineContext* ctx) final {
            m_Context = ctx;

            // Initialize helper objects
            transform = TransformAPI(ctx, ctx->CurrentEntity);
            rigidbody = Rigidbody2DAPI(ctx, ctx->CurrentEntity);
            rigidbody3d = Rigidbody3DAPI(ctx, ctx->CurrentEntity);
            input = InputAPI(ctx);
            time = TimeAPI(ctx);
            debug = DebugAPI(ctx);

            // Call user Start
            Start();
        }

        void OnUnload() final {
            OnDestroy();
        }

        void OnUpdate(float deltaTime) final {
            Update();
        }

        void OnPlayModeEnter() final {
            Start();
        }

        void OnPlayModeExit() final {
            OnDestroy();
        }

        std::vector<VarMetadata> GetPublicVariables() final {
            return m_PublicVariables;
        }

    protected:
        // Helper objects (ready to use!)
        TransformAPI transform;
        Rigidbody2DAPI rigidbody;
        Rigidbody3DAPI rigidbody3d;
        InputAPI input;
        TimeAPI time;
        DebugAPI debug;

        // Override these in your script
        virtual void Start() {}
        virtual void Update() {}
        virtual void OnDestroy() {}

        // Variable registration helpers
        void RegisterVar(const std::string& name, float* var) {
            m_PublicVariables.push_back(VarMetadata(name, VarType::Float, var));
        }

        void RegisterVar(const std::string& name, int* var) {
            m_PublicVariables.push_back(VarMetadata(name, VarType::Int, var));
        }

        void RegisterVar(const std::string& name, bool* var) {
            m_PublicVariables.push_back(VarMetadata(name, VarType::Bool, var));
        }

        void RegisterVar(const std::string& name, Vec2* var) {
            m_PublicVariables.push_back(VarMetadata(name, VarType::Vec2, var));
        }

        void RegisterVar(const std::string& name, Vec3* var) {
            m_PublicVariables.push_back(VarMetadata(name, VarType::Vec3, var));
        }

        void RegisterVar(const std::string& name, std::string* var) {
            m_PublicVariables.push_back(VarMetadata(name, VarType::String, var));
        }

        // Range-constrained registration
        void RegisterVarRange(const std::string& name, float* var, float min, float max) {
            VarMetadata meta(name, VarType::Float, var);
            meta.HasRange = true;
            meta.MinValue = min;
            meta.MaxValue = max;
            m_PublicVariables.push_back(meta);
        }

        // With tooltip
        void RegisterVarTooltip(const std::string& name, float* var, const std::string& tooltip) {
            VarMetadata meta(name, VarType::Float, var);
            meta.Tooltip = tooltip;
            m_PublicVariables.push_back(meta);
        }

    private:
        EngineContext* m_Context = nullptr;
        std::vector<VarMetadata> m_PublicVariables;
    };

} // namespace Lunex

// Funciones que deben exportar los plugins
extern "C"
{
    LUNEX_API uint32_t Lunex_GetScriptingAPIVersion();
    LUNEX_API Lunex::IScriptModule* Lunex_CreateModule();
    LUNEX_API void Lunex_DestroyModule(Lunex::IScriptModule* module);
}
