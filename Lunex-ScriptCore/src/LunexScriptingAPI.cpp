#include "LunexScriptingAPI.h"

namespace Lunex {

    // ========================================================================
    // TransformAPI Implementation
    // ========================================================================

    Vec3 TransformAPI::GetPosition() {
        Vec3 result;
        if (m_Context && m_Context->GetEntityPosition) {
            m_Context->GetEntityPosition(m_Entity, &result);
        }
        return result;
    }

    void TransformAPI::SetPosition(const Vec3& pos) {
        if (m_Context && m_Context->SetEntityPosition) {
            m_Context->SetEntityPosition(m_Entity, &pos);
        }
    }

    void TransformAPI::Translate(const Vec3& delta) {
        Vec3 current = GetPosition();
        SetPosition(current + delta);
    }

    Vec3 TransformAPI::GetRotation() {
        Vec3 result;
        if (m_Context && m_Context->GetEntityRotation) {
            m_Context->GetEntityRotation(m_Entity, &result);
        }
        return result;
    }

    void TransformAPI::SetRotation(const Vec3& rot) {
        if (m_Context && m_Context->SetEntityRotation) {
            m_Context->SetEntityRotation(m_Entity, &rot);
        }
    }

    void TransformAPI::Rotate(const Vec3& delta) {
        Vec3 current = GetRotation();
        SetRotation(current + delta);
    }

    Vec3 TransformAPI::GetScale() {
        Vec3 result;
        if (m_Context && m_Context->GetEntityScale) {
            m_Context->GetEntityScale(m_Entity, &result);
        }
        return result;
    }

    void TransformAPI::SetScale(const Vec3& scale) {
        if (m_Context && m_Context->SetEntityScale) {
            m_Context->SetEntityScale(m_Entity, &scale);
        }
    }

    // ========================================================================
    // Rigidbody2DAPI Implementation
    // ========================================================================

    bool Rigidbody2DAPI::Exists() {
        if (m_Context && m_Context->HasRigidbody2D) {
            return m_Context->HasRigidbody2D(m_Entity);
        }
        return false;
    }

    Vec2 Rigidbody2DAPI::GetVelocity() {
        Vec2 result;
        if (m_Context && m_Context->GetLinearVelocity) {
            m_Context->GetLinearVelocity(m_Entity, &result);
        }
        return result;
    }

    void Rigidbody2DAPI::SetVelocity(const Vec2& vel) {
        if (m_Context && m_Context->SetLinearVelocity) {
            m_Context->SetLinearVelocity(m_Entity, &vel);
        }
    }

    void Rigidbody2DAPI::AddVelocity(const Vec2& delta) {
        Vec2 current = GetVelocity();
        SetVelocity(current + delta);
    }

    void Rigidbody2DAPI::ApplyImpulse(const Vec2& impulse) {
        if (m_Context && m_Context->ApplyLinearImpulseToCenter) {
            m_Context->ApplyLinearImpulseToCenter(m_Entity, &impulse, true);
        }
    }

    void Rigidbody2DAPI::ApplyForce(const Vec2& force) {
        if (m_Context && m_Context->ApplyForceToCenter) {
            m_Context->ApplyForceToCenter(m_Entity, &force, true);
        }
    }

    float Rigidbody2DAPI::GetMass() {
        if (m_Context && m_Context->GetMass) {
            return m_Context->GetMass(m_Entity);
        }
        return 0.0f;
    }

    float Rigidbody2DAPI::GetGravityScale() {
        if (m_Context && m_Context->GetGravityScale) {
            return m_Context->GetGravityScale(m_Entity);
        }
        return 1.0f;
    }

    void Rigidbody2DAPI::SetGravityScale(float scale) {
        if (m_Context && m_Context->SetGravityScale) {
            m_Context->SetGravityScale(m_Entity, scale);
        }
    }

    // ========================================================================
    // InputAPI Implementation
    // ========================================================================

    bool InputAPI::IsKeyPressed(int key) {
        if (m_Context && m_Context->IsKeyPressed) {
            return m_Context->IsKeyPressed(key);
        }
        return false;
    }

    bool InputAPI::IsKeyDown(int key) {
        if (m_Context && m_Context->IsKeyDown) {
            return m_Context->IsKeyDown(key);
        }
        return false;
    }

    bool InputAPI::IsKeyReleased(int key) {
        if (m_Context && m_Context->IsKeyReleased) {
            return m_Context->IsKeyReleased(key);
        }
        return false;
    }

    bool InputAPI::IsMousePressed(int button) {
        if (m_Context && m_Context->IsMouseButtonPressed) {
            return m_Context->IsMouseButtonPressed(button);
        }
        return false;
    }

    Vec2 InputAPI::GetMousePosition() {
        Vec2 result;
        if (m_Context && m_Context->GetMousePosition) {
            m_Context->GetMousePosition(&result.x, &result.y);
        }
        return result;
    }

    // ========================================================================
    // TimeAPI Implementation
    // ========================================================================

    float TimeAPI::DeltaTime() {
        if (m_Context && m_Context->GetDeltaTime) {
            return m_Context->GetDeltaTime();
        }
        return 0.016f;
    }

    float TimeAPI::GetTime() {
        if (m_Context && m_Context->GetTime) {
            return m_Context->GetTime();
        }
        return 0.0f;
    }

    // ========================================================================
    // DebugAPI Implementation
    // ========================================================================

    void DebugAPI::Log(const std::string& message) {
        if (m_Context && m_Context->LogInfo) {
            m_Context->LogInfo(message.c_str());
        }
    }

    void DebugAPI::LogWarning(const std::string& message) {
        if (m_Context && m_Context->LogWarning) {
            m_Context->LogWarning(message.c_str());
        }
    }

    void DebugAPI::LogError(const std::string& message) {
        if (m_Context && m_Context->LogError) {
            m_Context->LogError(message.c_str());
        }
    }

} // namespace Lunex
