#include "stpch.h"
#include "ScriptingEngine.h"
#include "ScriptBuild.h"
#include "ScriptReflection.h"
#include "Core/Input.h"
#include "Scene/Entity.h"
#include "Physics/Physics.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <box2d/box2d.h>
#include <filesystem>
#include <fstream>

// Forward declare ConsolePanel for compilation messages
namespace Lunex {
	class ConsolePanel;
}

// Console logging helper - defined later to avoid header dependency
static void LogToConsole(const std::string& message, bool isError = false, bool isWarning = false);
static void LogCompileStart(const std::string& scriptName);
static void LogCompileSuccess(const std::string& scriptName);
static void LogCompileError(const std::string& scriptName, const std::string& error);
static void LogScriptMessage(const std::string& message, int level);

namespace Lunex {

	// Variables globales para el sistema de scripting (lambdas sin captura)
	static float g_CurrentDeltaTime = 0.016f;
	static Scene* g_CurrentScene = nullptr;

	// Script being loaded right now (its RegisterVar calls pick up inspector overrides)
	static size_t g_LoadingScriptIndex = 0;

	// Scene change requested from a script, consumed by the host (editor/runtime) at frame end
	static std::string s_PendingSceneLoad;
	static std::mutex s_PendingSceneMutex;

	ScriptingEngine::ScriptingEngine() {
	}

	ScriptingEngine::~ScriptingEngine() {
		// Limpiar scripts al destruir
		if (m_EngineContext) {
			for (auto& [uuid, plugin] : m_ScriptInstances) {
				if (plugin) {
					plugin->OnPlayModeExit();
					plugin->Unload();
				}
			}
			m_ScriptInstances.clear();
		}
	}

	void ScriptingEngine::Initialize(Scene* scene) {
		m_CurrentScene = scene;
		g_CurrentScene = scene;

		m_EngineContext = std::make_unique<EngineContext>();

		// === LOGGING ===
		m_EngineContext->LogInfo = [](const char* message) {
			LNX_LOG_INFO("[Script] {0}", message);
		};

		m_EngineContext->LogWarning = [](const char* message) {
			LNX_LOG_WARN("[Script] {0}", message);
		};

		m_EngineContext->LogError = [](const char* message) {
			LNX_LOG_ERROR("[Script] {0}", message);
		};

		// === TIME ===
		m_EngineContext->GetDeltaTime = []() -> float {
			return g_CurrentDeltaTime;
		};

		m_EngineContext->GetTime = []() -> float {
			return (float)glfwGetTime();
		};

		// === ENTITY MANAGEMENT ===
		m_EngineContext->CreateEntity = [](const char* name) -> void* {
			if (!g_CurrentScene) return nullptr;
			Entity newEntity = g_CurrentScene->CreateEntity(name);
			auto entityHandle = (entt::entity)newEntity;
			return reinterpret_cast<void*>(static_cast<uintptr_t>(entityHandle));
		};

		m_EngineContext->DestroyEntity = [](void* entity) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			g_CurrentScene->DestroyEntity(ent);
		};

		// === TRANSFORM COMPONENT ===
		m_EngineContext->GetEntityPosition = [](void* entity, Vec3* outPosition) {
			if (!entity || !g_CurrentScene || !outPosition) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outPosition->x = transform.Translation.x;
				outPosition->y = transform.Translation.y;
				outPosition->z = transform.Translation.z;
			}
		};

		m_EngineContext->SetEntityPosition = [](void* entity, const Vec3* position) {
			if (!entity || !g_CurrentScene || !position) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Translation.x = position->x;
				transform.Translation.y = position->y;
				transform.Translation.z = position->z;
			}
		};

		m_EngineContext->GetEntityRotation = [](void* entity, Vec3* outRotation) {
			if (!entity || !g_CurrentScene || !outRotation) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outRotation->x = transform.Rotation.x;
				outRotation->y = transform.Rotation.y;
				outRotation->z = transform.Rotation.z;
			}
		};

		m_EngineContext->SetEntityRotation = [](void* entity, const Vec3* rotation) {
			if (!entity || !g_CurrentScene || !rotation) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Rotation.x = rotation->x;
				transform.Rotation.y = rotation->y;
				transform.Rotation.z = rotation->z;
			}
		};

		m_EngineContext->GetEntityScale = [](void* entity, Vec3* outScale) {
			if (!entity || !g_CurrentScene || !outScale) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				outScale->x = transform.Scale.x;
				outScale->y = transform.Scale.y;
				outScale->z = transform.Scale.z;
			}
		};

		m_EngineContext->SetEntityScale = [](void* entity, const Vec3* scale) {
			if (!entity || !g_CurrentScene || !scale) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			if (ent.HasComponent<TransformComponent>()) {
				auto& transform = ent.GetComponent<TransformComponent>();
				transform.Scale.x = scale->x;
				transform.Scale.y = scale->y;
				transform.Scale.z = scale->z;
			}
		};

		// === INPUT SYSTEM ===
		m_EngineContext->IsKeyPressed = [](int keyCode) -> bool {
			return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsKeyDown = [](int keyCode) -> bool {
			return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsKeyReleased = [](int keyCode) -> bool {
			return !Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
		};

		m_EngineContext->IsMouseButtonPressed = [](int button) -> bool {
			return Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->IsMouseButtonDown = [](int button) -> bool {
			return Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->IsMouseButtonReleased = [](int button) -> bool {
			return !Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
		};

		m_EngineContext->GetMousePosition = [](float* outX, float* outY) {
			if (!outX || !outY) return;
			auto pos = Input::GetMousePosition();
			*outX = pos.first;
			*outY = pos.second;
		};

		m_EngineContext->GetMouseX = []() -> float {
			return Input::GetMouseX();
		};

		m_EngineContext->GetMouseY = []() -> float {
			return Input::GetMouseY();
		};

		// === RIGIDBODY2D COMPONENT ===
		m_EngineContext->HasRigidbody2D = [](void* entity) -> bool {
			if (!entity || !g_CurrentScene) return false;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			return ent.HasComponent<Rigidbody2DComponent>();
		};

		m_EngineContext->GetLinearVelocity = [](void* entity, Vec2* outVelocity) {
			if (!entity || !g_CurrentScene || !outVelocity) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					b2Vec2 velocity = b2Body_GetLinearVelocity(bodyId);
					outVelocity->x = velocity.x;
					outVelocity->y = velocity.y;
				}
			}
		};

		m_EngineContext->SetLinearVelocity = [](void* entity, const Vec2* velocity) {
			if (!entity || !g_CurrentScene || !velocity) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					b2Vec2 vel = { velocity->x, velocity->y };
					b2Body_SetLinearVelocity(bodyId, vel);
				}
			}
		};

		m_EngineContext->ApplyLinearImpulse = [](void* entity, const Vec2* impulse, bool wake) {
			if (!entity || !g_CurrentScene || !impulse) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					b2Vec2 imp = { impulse->x, impulse->y };
					b2Vec2 point = b2Body_GetPosition(bodyId);
					b2Body_ApplyLinearImpulse(bodyId, imp, point, wake);
				}
			}
		};

		m_EngineContext->ApplyLinearImpulseToCenter = [](void* entity, const Vec2* impulse, bool wake) {
			if (!entity || !g_CurrentScene || !impulse) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					b2Vec2 imp = { impulse->x, impulse->y };
					b2Vec2 point = b2Body_GetPosition(bodyId);
					b2Body_ApplyLinearImpulse(bodyId, imp, point, wake);
				}
			}
		};

		m_EngineContext->ApplyForce = [](void* entity, const Vec2* force, const Vec2* point, bool wake) {
			if (!entity || !g_CurrentScene || !force || !point) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					b2Vec2 f = { force->x, force->y };
					b2Vec2 p = { point->x, point->y };
					b2Body_ApplyForce(bodyId, f, p, wake);
				}
			}
		};

		m_EngineContext->ApplyForceToCenter = [](void* entity, const Vec2* force, bool wake) {
			if (!entity || !g_CurrentScene || !force) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					b2Vec2 f = { force->x, force->y };
					b2Vec2 point = b2Body_GetPosition(bodyId);
					b2Body_ApplyForce(bodyId, f, point, wake);
				}
			}
		};

		m_EngineContext->GetMass = [](void* entity) -> float {
			if (!entity || !g_CurrentScene) return 0.0f;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					return b2Body_GetMass(bodyId);
				}
			}
			return 0.0f;
		};

		m_EngineContext->GetGravityScale = [](void* entity) -> float {
			if (!entity || !g_CurrentScene) return 1.0f;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					return b2Body_GetGravityScale(bodyId);
				}
			}
			return 1.0f;
		};

		m_EngineContext->SetGravityScale = [](void* entity, float scale) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody2DComponent>()) {
				auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
				if (rb2d.RuntimeBody) {
					b2BodyId bodyId = *static_cast<b2BodyId*>(rb2d.RuntimeBody);
					b2Body_SetGravityScale(bodyId, scale);
				}
			}
		};

		// === RIGIDBODY3D COMPONENT (Bullet3) ===
		m_EngineContext->HasRigidbody3D = [](void* entity) -> bool {
			if (!entity || !g_CurrentScene) return false;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			return ent.HasComponent<Rigidbody3DComponent>();
		};

		m_EngineContext->GetLinearVelocity3D = [](void* entity, Vec3* outVelocity) {
			if (!entity || !g_CurrentScene || !outVelocity) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					glm::vec3 vel = body->GetLinearVelocity();
					*outVelocity = Vec3(vel);
				}
			}
		};

		m_EngineContext->SetLinearVelocity3D = [](void* entity, const Vec3* velocity) {
			if (!entity || !g_CurrentScene || !velocity) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetLinearVelocity(glm::vec3(*velocity));
				}
			}
		};

		m_EngineContext->GetAngularVelocity3D = [](void* entity, Vec3* outVelocity) {
			if (!entity || !g_CurrentScene || !outVelocity) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					glm::vec3 vel = body->GetAngularVelocity();
					*outVelocity = Vec3(vel);
				}
			}
		};

		m_EngineContext->SetAngularVelocity3D = [](void* entity, const Vec3* velocity) {
			if (!entity || !g_CurrentScene || !velocity) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetAngularVelocity(glm::vec3(*velocity));
				}
			}
		};

		m_EngineContext->ApplyForce3D = [](void* entity, const Vec3* force) {
			if (!entity || !g_CurrentScene || !force) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->ApplyForce(glm::vec3(*force));
				}
			}
		};

		m_EngineContext->ApplyForceAtPoint3D = [](void* entity, const Vec3* force, const Vec3* point) {
			if (!entity || !g_CurrentScene || !force || !point) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->ApplyForce(glm::vec3(*force), glm::vec3(*point));
				}
			}
		};

		m_EngineContext->ApplyImpulse3D = [](void* entity, const Vec3* impulse) {
			if (!entity || !g_CurrentScene || !impulse) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->ApplyImpulse(glm::vec3(*impulse));
				}
			}
		};

		m_EngineContext->ApplyImpulseAtPoint3D = [](void* entity, const Vec3* impulse, const Vec3* point) {
			if (!entity || !g_CurrentScene || !impulse || !point) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->ApplyImpulse(glm::vec3(*impulse), glm::vec3(*point));
				}
			}
		};

		m_EngineContext->ApplyTorque3D = [](void* entity, const Vec3* torque) {
			if (!entity || !g_CurrentScene || !torque) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->ApplyTorque(glm::vec3(*torque));
				}
			}
		};

		m_EngineContext->ApplyTorqueImpulse3D = [](void* entity, const Vec3* torque) {
			if (!entity || !g_CurrentScene || !torque) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->ApplyTorqueImpulse(glm::vec3(*torque));
				}
			}
		};

		m_EngineContext->GetMass3D = [](void* entity) -> float {
			if (!entity || !g_CurrentScene) return 0.0f;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					return body->GetMass();
				}
			}
			return 0.0f;
		};

		m_EngineContext->SetMass3D = [](void* entity, float mass) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				rb3d.Mass = mass;
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetMass(mass);
				}
			}
		};

		m_EngineContext->GetFriction3D = [](void* entity) -> float {
			if (!entity || !g_CurrentScene) return 0.5f;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				return rb3d.Friction;
			}
			return 0.5f;
		};

		m_EngineContext->SetFriction3D = [](void* entity, float friction) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				rb3d.Friction = friction;
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetFriction(friction);
				}
			}
		};

		m_EngineContext->GetRestitution3D = [](void* entity) -> float {
			if (!entity || !g_CurrentScene) return 0.0f;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				return rb3d.Restitution;
			}
			return 0.0f;
		};

		m_EngineContext->SetRestitution3D = [](void* entity, float restitution) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				rb3d.Restitution = restitution;
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetRestitution(restitution);
				}
			}
		};

		m_EngineContext->GetLinearDamping3D = [](void* entity) -> float {
			if (!entity || !g_CurrentScene) return 0.0f;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				return rb3d.LinearDamping;
			}
			return 0.0f;
		};

		m_EngineContext->SetLinearDamping3D = [](void* entity, float damping) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				rb3d.LinearDamping = damping;
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					// SetDamping takes both linear and angular, so preserve angular
					body->SetDamping(damping, rb3d.AngularDamping);
				}
			}
		};

		m_EngineContext->GetAngularDamping3D = [](void* entity) -> float {
			if (!entity || !g_CurrentScene) return 0.0f;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				return rb3d.AngularDamping;
			}
			return 0.0f;
		};

		m_EngineContext->SetAngularDamping3D = [](void* entity, float damping) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				rb3d.AngularDamping = damping;
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					// SetDamping takes both linear and angular, so preserve linear
					body->SetDamping(rb3d.LinearDamping, damping);
				}
			}
		};

		m_EngineContext->SetLinearFactor3D = [](void* entity, const Vec3* factor) {
			if (!entity || !g_CurrentScene || !factor) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				rb3d.LinearFactor = glm::vec3(*factor);
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetLinearFactor(glm::vec3(*factor));
				}
			}
		};

		m_EngineContext->GetLinearFactor3D = [](void* entity, Vec3* outFactor) {
			if (!entity || !g_CurrentScene || !outFactor) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				*outFactor = Vec3(rb3d.LinearFactor);
			}
		};

		m_EngineContext->SetAngularFactor3D = [](void* entity, const Vec3* factor) {
			if (!entity || !g_CurrentScene || !factor) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				rb3d.AngularFactor = glm::vec3(*factor);
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetAngularFactor(glm::vec3(*factor));
				}
			}
		};

		m_EngineContext->GetAngularFactor3D = [](void* entity, Vec3* outFactor) {
			if (!entity || !g_CurrentScene || !outFactor) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				*outFactor = Vec3(rb3d.AngularFactor);
			}
		};

		m_EngineContext->ClearForces3D = [](void* entity) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					// Bullet clears forces by setting them to zero
					btRigidBody* btBody = body->GetRigidBody();
					if (btBody) {
						btBody->clearForces();
					}
				}
			}
		};

		m_EngineContext->Activate3D = [](void* entity) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->Activate();
				}
			}
		};

		m_EngineContext->Deactivate3D = [](void* entity) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					// SetActivationState with WANTS_DEACTIVATION
					body->SetActivationState(WANTS_DEACTIVATION);
				}
			}
		};

		m_EngineContext->IsActive3D = [](void* entity) -> bool {
			if (!entity || !g_CurrentScene) return false;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					return body->IsActive();
				}
			}
			return false;
		};

		m_EngineContext->IsKinematic3D = [](void* entity) -> bool {
			if (!entity || !g_CurrentScene) return false;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					return body->IsKinematic();
				}
			}
			return false;
		};

		m_EngineContext->SetKinematic3D = [](void* entity, bool kinematic) {
			if (!entity || !g_CurrentScene) return;
			auto entityHandle = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity));
			Entity ent{ entityHandle, g_CurrentScene };
			
			if (ent.HasComponent<Rigidbody3DComponent>()) {
				auto& rb3d = ent.GetComponent<Rigidbody3DComponent>();
				if (rb3d.RuntimeBody) {
					RigidBodyComponent* body = static_cast<RigidBodyComponent*>(rb3d.RuntimeBody);
					body->SetKinematic(kinematic);
				}
			}
		};

		// === SCENE ACCESS ===
		m_EngineContext->FindEntityByName = [](const char* name) -> void* {
			if (!name || !g_CurrentScene) return nullptr;
			auto view = g_CurrentScene->GetAllEntitiesWith<TagComponent>();
			for (auto e : view) {
				Entity ent{ e, g_CurrentScene };
				if (ent.GetComponent<TagComponent>().Tag == name) {
					return reinterpret_cast<void*>(static_cast<uintptr_t>(e));
				}
			}
			return nullptr;
		};

		// === INSPECTOR OVERRIDES FOR PUBLIC VARIABLES ===
		m_EngineContext->GetFieldOverride = [](void* entity, const char* name, int type, float* outNumbers,
			char* outString, int outStringSize) -> bool {
			if (!entity || !g_CurrentScene || !name) return false;
			Entity ent{ static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity)), g_CurrentScene };
			if (!ent.HasComponent<ScriptComponent>()) return false;
			auto& sc = ent.GetComponent<ScriptComponent>();
			if (g_LoadingScriptIndex >= sc.FieldOverrides.size()) return false;
			auto& overrides = sc.FieldOverrides[g_LoadingScriptIndex];
			auto it = overrides.find(name);
			if (it == overrides.end() || static_cast<int>(it->second.Type) != type) return false;
			for (int i = 0; i < 4; i++) outNumbers[i] = it->second.Number[i];
			if (outString && outStringSize > 0) {
				strncpy_s(outString, static_cast<size_t>(outStringSize), it->second.String.c_str(), _TRUNCATE);
			}
			return true;
		};

		m_EngineContext->PlayAnimation = [](void* entity, int clipIndex) {
			if (!entity || !g_CurrentScene) return;
			Entity ent{ static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity)), g_CurrentScene };
			if (ent.HasComponent<AnimatorComponent>()) ent.GetComponent<AnimatorComponent>().Play(clipIndex);
		};

		m_EngineContext->StopAnimation = [](void* entity) {
			if (!entity || !g_CurrentScene) return;
			Entity ent{ static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity)), g_CurrentScene };
			if (ent.HasComponent<AnimatorComponent>()) ent.GetComponent<AnimatorComponent>().Stop();
		};

		// ---------------- Materials ----------------
		// Every override goes to the entity's own MaterialInstance (cloned first if shared)
		static auto materialOf = [](void* entity) -> MaterialInstance* {
			if (!entity || !g_CurrentScene) return nullptr;
			Entity ent{ static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity)), g_CurrentScene };
			if (!ent.HasComponent<MaterialComponent>()) return nullptr;
			auto& mc = ent.GetComponent<MaterialComponent>();
			if (!mc.Instance) return nullptr;
			if (mc.Instance.use_count() > 1) mc.Instance = mc.Instance->Clone();
			return mc.Instance.get();
		};

		m_EngineContext->SetMaterialColor = [](void* entity, const float* rgba) {
			if (auto* m = materialOf(entity); m && rgba) m->SetAlbedo(glm::vec4(rgba[0], rgba[1], rgba[2], rgba[3]), true);
		};
		m_EngineContext->GetMaterialColor = [](void* entity, float* out) {
			if (auto* m = materialOf(entity); m && out) {
				const glm::vec4 c = m->GetAlbedo();
				out[0] = c.r; out[1] = c.g; out[2] = c.b; out[3] = c.a;
			}
		};
		m_EngineContext->SetMaterialFloat = [](void* entity, const char* property, float value) {
			auto* m = materialOf(entity);
			if (!m || !property) return;
			const std::string p = property;
			if (p == "Metallic") m->SetMetallic(value, true);
			else if (p == "Roughness") m->SetRoughness(value, true);
			else if (p == "Specular") m->SetSpecular(value, true);
			else if (p == "EmissionIntensity") m->SetEmissionIntensity(value, true);
			else if (p == "NormalIntensity") m->SetNormalIntensity(value, true);
			else m->SetGraphParameter(p, glm::vec4(value));   // unknown name: node-graph parameter
		};
		m_EngineContext->GetMaterialFloat = [](void* entity, const char* property) -> float {
			auto* m = materialOf(entity);
			if (!m || !property) return 0.0f;
			const std::string p = property;
			if (p == "Metallic") return m->GetMetallic();
			if (p == "Roughness") return m->GetRoughness();
			if (p == "Specular") return m->GetSpecular();
			if (p == "EmissionIntensity") return m->GetEmissionIntensity();
			if (p == "NormalIntensity") return m->GetNormalIntensity();
			glm::vec4 v;
			return m->GetGraphParameter(p, v) ? v.x : 0.0f;
		};
		m_EngineContext->SetMaterialEmission = [](void* entity, const float* rgb, float intensity) {
			if (auto* m = materialOf(entity); m && rgb) {
				m->SetEmissionColor(glm::vec3(rgb[0], rgb[1], rgb[2]), true);
				m->SetEmissionIntensity(intensity, true);
			}
		};
		m_EngineContext->SetMaterialParameter = [](void* entity, const char* name, const float* rgba) {
			if (auto* m = materialOf(entity); m && name && rgba) m->SetGraphParameter(name, glm::vec4(rgba[0], rgba[1], rgba[2], rgba[3]));
		};
		m_EngineContext->ResetMaterial = [](void* entity) {
			if (auto* m = materialOf(entity)) m->ResetOverrides();
		};

		// ---------------- Particles ----------------
		static auto particlesOf = [](void* entity) -> ParticleSystemComponent* {
			if (!entity || !g_CurrentScene) return nullptr;
			Entity ent{ static_cast<entt::entity>(reinterpret_cast<uintptr_t>(entity)), g_CurrentScene };
			return ent.HasComponent<ParticleSystemComponent>() ? &ent.GetComponent<ParticleSystemComponent>() : nullptr;
		};
		m_EngineContext->EmitParticles = [](void* entity, int count) {
			if (auto* ps = particlesOf(entity)) ps->Emit(count);
		};
		m_EngineContext->SetParticlesPlaying = [](void* entity, bool playing) {
			if (auto* ps = particlesOf(entity)) { if (playing) ps->Play(); else ps->Stop(); }
		};
		m_EngineContext->SetParticleRate = [](void* entity, float rate) {
			if (auto* ps = particlesOf(entity)) ps->Rate = std::max(0.0f, rate);
		};

		m_EngineContext->RequestLoadScene = [](const char* scenePath) {
			if (!scenePath) return;
			std::scoped_lock lock(s_PendingSceneMutex);
			s_PendingSceneLoad = scenePath;
		};

		// CurrentEntity se establecerá cuando se cargue el script

		for (int i = 0; i < static_cast<int>(std::size(m_EngineContext->reserved)); ++i) {
			m_EngineContext->reserved[i] = nullptr;
		}
	}

	bool ScriptingEngine::ConsumePendingSceneLoad(std::string& outScenePath) {
		std::scoped_lock lock(s_PendingSceneMutex);
		if (s_PendingSceneLoad.empty()) return false;
		outScenePath = std::move(s_PendingSceneLoad);
		s_PendingSceneLoad.clear();
		return true;
	}

	void ScriptingEngine::OnScriptsStart(entt::registry& registry) {
		// Script callbacks resolve entities through g_CurrentScene: bind it to *this* engine's scene
		g_CurrentScene = m_CurrentScene;
// ===== PARALLELIZED SCRIPT COMPILATION =====
		// Collect all scripts that need compilation
		struct ScriptCompileTask {
			uint32_t EntityRaw;  // Store as uint32_t instead of entt::entity
			size_t ScriptIndex;
			std::string ScriptPath;
			uint64_t EntityID;
		};
		
		std::vector<ScriptCompileTask> compileTasks;
		
		// Iterar por todas las entidades con ScriptComponent
		auto view = registry.view<ScriptComponent, IDComponent>();
		view.each([&](auto entityHandle, auto& scriptComp, auto& idComp) {
			// ===== ITERAR POR TODOS LOS SCRIPTS DE LA ENTIDAD =====
			for (size_t i = 0; i < scriptComp.GetScriptCount(); i++) {
				const std::string& scriptPath = scriptComp.GetScriptPath(i);
				
				if (scriptPath.empty()) {
					continue;
				}

				if (scriptComp.AutoCompile) {
					compileTasks.push_back({ 
						static_cast<uint32_t>(entityHandle), 
						i, 
						scriptPath, 
						static_cast<uint64_t>(idComp.ID)
					});
				}
			}
		});

		// ===== COMPILE ALL SCRIPTS IN PARALLEL =====
		if (!compileTasks.empty()) {
			LNX_LOG_INFO("?? Compiling {0} scripts in parallel...", compileTasks.size());
			
			// Use shared_ptr to safely share data across threads
			auto compiledResults = std::make_shared<std::vector<std::pair<size_t, std::string>>>(compileTasks.size());
			
			auto counter = JobSystem::Get().CreateCounter(static_cast<int32_t>(compileTasks.size()));
			
			for (size_t taskIdx = 0; taskIdx < compileTasks.size(); ++taskIdx) {
				const auto& task = compileTasks[taskIdx];
				
				JobSystem::Get().Schedule(
					[this, task, taskIdx, compiledResults]() {
						std::string dllPath;
						if (CompileScript(task.ScriptPath, dllPath)) {
							(*compiledResults)[taskIdx] = { taskIdx, dllPath };
							LNX_LOG_INFO("? Script #{0} compiled: {1}", task.ScriptIndex + 1, dllPath);
						}
						else {
							(*compiledResults)[taskIdx] = { taskIdx, "" };
							LNX_LOG_ERROR("? Failed to compile script #{0}: {1}", task.ScriptIndex + 1, task.ScriptPath);
						}
					},
					nullptr,
					counter,
					JobPriority::High  // High priority for compilation
				);
			}
			
			// Wait for all compilations to complete
			JobSystem::Get().Wait(counter);
			
			// Apply results back to components (main thread only)
			for (size_t i = 0; i < compileTasks.size(); ++i) {
				const auto& task = compileTasks[i];
				const auto& [idx, dllPath] = (*compiledResults)[i];
				
				// Reconstruct entity handle
				entt::entity entity = static_cast<entt::entity>(task.EntityRaw);
				
				// Find entity again (might have been destroyed)
				if (!registry.valid(entity)) {
					continue;
				}
				
				auto& scriptComp = registry.get<ScriptComponent>(entity);
				
				if (!dllPath.empty()) {
					// Update DLL path in the component
					if (task.ScriptIndex < scriptComp.CompiledDLLPaths.size()) {
						scriptComp.CompiledDLLPaths[task.ScriptIndex] = dllPath;
					}
				}
			}
			
			LNX_LOG_INFO("? All scripts compiled in parallel");
		}

		// ===== LOAD COMPILED SCRIPTS (sequential for now, DLL loading is not thread-safe) =====
		view.each([&](auto entityHandle, auto& scriptComp, auto& idComp) {
			for (size_t i = 0; i < scriptComp.GetScriptCount(); i++) {
				const std::string& scriptPath = scriptComp.GetScriptPath(i);
				
				if (scriptPath.empty()) {
					continue;
				}

				// Si no hay DLL compilada, saltar
				if (i >= scriptComp.CompiledDLLPaths.size() || scriptComp.CompiledDLLPaths[i].empty()) {
					LNX_LOG_WARN("No compiled DLL for script #{0}: {1}", i + 1, scriptPath);
					continue;
				}

				// Cargar script
				auto plugin = std::make_unique<ScriptPlugin>();

				// Establecer CurrentEntity antes de cargar el script
				m_EngineContext->CurrentEntity = reinterpret_cast<void*>(static_cast<uintptr_t>(entityHandle));
				g_LoadingScriptIndex = i;

				// Load a private copy so the original DLL can be recompiled while playing
				std::string loadPath = ScriptBuild::MakeLoadableCopy(scriptComp.CompiledDLLPaths[i]);
				if (plugin->Load(loadPath, m_EngineContext.get())) {
					plugin->OnPlayModeEnter();
					
					// Actualizar estado en el componente
					if (i < scriptComp.ScriptLoadedStates.size()) {
						scriptComp.ScriptLoadedStates[i] = true;
					}
					if (i < scriptComp.ScriptPluginInstances.size()) {
						scriptComp.ScriptPluginInstances[i] = plugin.get();
					}

					// Guardar el plugin en el mapa usando UUID + índice
					uint64_t uniqueKey = ((uint64_t)idComp.ID << 32) | i;
					m_ScriptInstances[uniqueKey] = std::move(plugin);

					LNX_LOG_INFO("Script #{0} loaded and started: {1}", i + 1, scriptComp.CompiledDLLPaths[i]);
				}
				else {
					LNX_LOG_ERROR("Failed to load script #{0}: {1}", i + 1, scriptComp.CompiledDLLPaths[i]);
				}
			}
		});
	}

	void ScriptingEngine::OnScriptsStop(entt::registry& registry) {
		g_CurrentScene = m_CurrentScene;
// Detener todos los scripts
		for (auto& [uuid, plugin] : m_ScriptInstances) {
			if (plugin) {
				plugin->OnPlayModeExit();
				plugin->Unload();
			}
		}

		// Limpiar el mapa
		m_ScriptInstances.clear();

		// Remove the private DLL copies loaded this session (FreeLibrary already happened)
		registry.view<ScriptComponent>().each([](auto, auto& sc) {
			for (const auto& dll : sc.CompiledDLLPaths) {
				if (!dll.empty()) ScriptBuild::CleanupLoadableCopies(dll);
			}
		});

		// Marcar todos los componentes como descargados
		auto view = registry.view<ScriptComponent>();
		view.each([&](auto entityHandle, auto& scriptComp) {
			// Limpiar estados de todos los scripts
			for (size_t i = 0; i < scriptComp.ScriptLoadedStates.size(); i++) {
				scriptComp.ScriptLoadedStates[i] = false;
			}
			for (size_t i = 0; i < scriptComp.ScriptPluginInstances.size(); i++) {
				scriptComp.ScriptPluginInstances[i] = nullptr;
			}
		});
	}

	void ScriptingEngine::OnScriptsUpdate(float deltaTime) {
		// Actualizar deltaTime global
		g_CurrentDeltaTime = deltaTime;
		g_CurrentScene = m_CurrentScene;

		if (m_ScriptInstances.empty()) {
			return;
		}
		
		// Collect all valid scripts
		std::vector<ScriptPlugin*> activeScripts;
		activeScripts.reserve(m_ScriptInstances.size());
		
		for (auto& [uuid, plugin] : m_ScriptInstances) {
			if (plugin && plugin->IsLoaded()) {
				activeScripts.push_back(plugin.get());
			}
		}
		
		if (activeScripts.empty()) {
			return;
		}
		
		// Update all scripts sequentially - scripts access the entt registry
		// via EngineContext callbacks which is NOT thread-safe
		for (auto* plugin : activeScripts) {
			if (plugin && plugin->IsLoaded()) {
				plugin->Update(deltaTime);
			}
		}
	}

	bool ScriptingEngine::CompileScript(const std::string& scriptPath, std::string& outDLLPath) {
		// Robust pipeline: path resolution, MSVC discovery, per-script temp dirs, status for the UI
		return ScriptBuild::Compile(ScriptBuild::ResolveScriptPath(scriptPath), outDLLPath);
	}

}
