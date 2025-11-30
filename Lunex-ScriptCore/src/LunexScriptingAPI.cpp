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
    // Rigidbody3DAPI Implementation (Bullet3)
    // ========================================================================

    bool Rigidbody3DAPI::Exists() {
        if (m_Context && m_Context->HasRigidbody3D) {
            return m_Context->HasRigidbody3D(m_Entity);
        }
        return false;
    }

    Vec3 Rigidbody3DAPI::GetLinearVelocity() {
        Vec3 result;
        if (m_Context && m_Context->GetLinearVelocity3D) {
            m_Context->GetLinearVelocity3D(m_Entity, &result);
        }
        return result;
    }

    void Rigidbody3DAPI::SetLinearVelocity(const Vec3& vel) {
        if (m_Context && m_Context->SetLinearVelocity3D) {
            m_Context->SetLinearVelocity3D(m_Entity, &vel);
        }
    }

    void Rigidbody3DAPI::AddLinearVelocity(const Vec3& delta) {
        Vec3 current = GetLinearVelocity();
        SetLinearVelocity(current + delta);
    }

    Vec3 Rigidbody3DAPI::GetAngularVelocity() {
        Vec3 result;
        if (m_Context && m_Context->GetAngularVelocity3D) {
            m_Context->GetAngularVelocity3D(m_Entity, &result);
        }
        return result;
    }

    void Rigidbody3DAPI::SetAngularVelocity(const Vec3& vel) {
        if (m_Context && m_Context->SetAngularVelocity3D) {
            m_Context->SetAngularVelocity3D(m_Entity, &vel);
        }
    }

    void Rigidbody3DAPI::ApplyForce(const Vec3& force) {
        if (m_Context && m_Context->ApplyForce3D) {
            m_Context->ApplyForce3D(m_Entity, &force);
        }
    }

    void Rigidbody3DAPI::ApplyForceAtPoint(const Vec3& force, const Vec3& point) {
        if (m_Context && m_Context->ApplyForceAtPoint3D) {
            m_Context->ApplyForceAtPoint3D(m_Entity, &force, &point);
        }
    }

    void Rigidbody3DAPI::ApplyImpulse(const Vec3& impulse) {
        if (m_Context && m_Context->ApplyImpulse3D) {
            m_Context->ApplyImpulse3D(m_Entity, &impulse);
        }
    }

    void Rigidbody3DAPI::ApplyImpulseAtPoint(const Vec3& impulse, const Vec3& point) {
        if (m_Context && m_Context->ApplyImpulseAtPoint3D) {
            m_Context->ApplyImpulseAtPoint3D(m_Entity, &impulse, &point);
        }
    }

    void Rigidbody3DAPI::ApplyTorque(const Vec3& torque) {
        if (m_Context && m_Context->ApplyTorque3D) {
            m_Context->ApplyTorque3D(m_Entity, &torque);
        }
    }

    void Rigidbody3DAPI::ApplyTorqueImpulse(const Vec3& torque) {
        if (m_Context && m_Context->ApplyTorqueImpulse3D) {
            m_Context->ApplyTorqueImpulse3D(m_Entity, &torque);
        }
    }

    float Rigidbody3DAPI::GetMass() {
        if (m_Context && m_Context->GetMass3D) {
            return m_Context->GetMass3D(m_Entity);
        }
        return 0.0f;
    }

    void Rigidbody3DAPI::SetMass(float mass) {
        if (m_Context && m_Context->SetMass3D) {
            m_Context->SetMass3D(m_Entity, mass);
        }
    }

    float Rigidbody3DAPI::GetFriction() {
        if (m_Context && m_Context->GetFriction3D) {
            return m_Context->GetFriction3D(m_Entity);
        }
        return 0.5f;
    }

    void Rigidbody3DAPI::SetFriction(float friction) {
        if (m_Context && m_Context->SetFriction3D) {
            m_Context->SetFriction3D(m_Entity, friction);
        }
    }

    float Rigidbody3DAPI::GetRestitution() {
        if (m_Context && m_Context->GetRestitution3D) {
            return m_Context->GetRestitution3D(m_Entity);
        }
        return 0.0f;
    }

    void Rigidbody3DAPI::SetRestitution(float restitution) {
        if (m_Context && m_Context->SetRestitution3D) {
            m_Context->SetRestitution3D(m_Entity, restitution);
        }
    }

    float Rigidbody3DAPI::GetLinearDamping() {
        if (m_Context && m_Context->GetLinearDamping3D) {
            return m_Context->GetLinearDamping3D(m_Entity);
        }
        return 0.0f;
    }

    void Rigidbody3DAPI::SetLinearDamping(float damping) {
        if (m_Context && m_Context->SetLinearDamping3D) {
            m_Context->SetLinearDamping3D(m_Entity, damping);
        }
    }

    float Rigidbody3DAPI::GetAngularDamping() {
        if (m_Context && m_Context->GetAngularDamping3D) {
            return m_Context->GetAngularDamping3D(m_Entity);
        }
        return 0.0f;
    }

    void Rigidbody3DAPI::SetAngularDamping(float damping) {
        if (m_Context && m_Context->SetAngularDamping3D) {
            m_Context->SetAngularDamping3D(m_Entity, damping);
        }
    }

    void Rigidbody3DAPI::SetLinearFactor(const Vec3& factor) {
        if (m_Context && m_Context->SetLinearFactor3D) {
            m_Context->SetLinearFactor3D(m_Entity, &factor);
        }
    }

    Vec3 Rigidbody3DAPI::GetLinearFactor() {
        Vec3 result;
        if (m_Context && m_Context->GetLinearFactor3D) {
            m_Context->GetLinearFactor3D(m_Entity, &result);
        }
        return result;
    }

    void Rigidbody3DAPI::SetAngularFactor(const Vec3& factor) {
        if (m_Context && m_Context->SetAngularFactor3D) {
            m_Context->SetAngularFactor3D(m_Entity, &factor);
        }
    }

    Vec3 Rigidbody3DAPI::GetAngularFactor() {
        Vec3 result;
        if (m_Context && m_Context->GetAngularFactor3D) {
            m_Context->GetAngularFactor3D(m_Entity, &result);
        }
        return result;
    }

    void Rigidbody3DAPI::SetKinematic(bool kinematic) {
        if (m_Context && m_Context->SetKinematic3D) {
            m_Context->SetKinematic3D(m_Entity, kinematic);
        }
    }

    bool Rigidbody3DAPI::IsKinematic() {
        if (m_Context && m_Context->IsKinematic3D) {
            return m_Context->IsKinematic3D(m_Entity);
        }
        return false;
    }

    void Rigidbody3DAPI::ClearForces() {
        if (m_Context && m_Context->ClearForces3D) {
            m_Context->ClearForces3D(m_Entity);
        }
    }

    void Rigidbody3DAPI::Activate() {
        if (m_Context && m_Context->Activate3D) {
            m_Context->Activate3D(m_Entity);
        }
    }

    void Rigidbody3DAPI::Deactivate() {
        if (m_Context && m_Context->Deactivate3D) {
            m_Context->Deactivate3D(m_Entity);
        }
    }

    bool Rigidbody3DAPI::IsActive() {
        if (m_Context && m_Context->IsActive3D) {
            return m_Context->IsActive3D(m_Entity);
        }
        return false;
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
