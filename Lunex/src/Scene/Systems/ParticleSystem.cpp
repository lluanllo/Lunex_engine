#include "stpch.h"
#include "ParticleSystem.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Asset/Prefab.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Physics/Components/RigidBodyComponent.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/Deferred/DeferredRenderer.h"
#include "Project/Project.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Lunex {

	using PSC = ParticleSystemComponent;

	namespace {
		float Rand01(uint32_t& state) {
			// xorshift32
			state ^= state << 13; state ^= state >> 17; state ^= state << 5;
			return static_cast<float>(state & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
		}
		float RandRange(uint32_t& state, const glm::vec2& range) { return range.x + (range.y - range.x) * Rand01(state); }

		glm::vec3 RandomUnitVector(uint32_t& s) {
			const float z = Rand01(s) * 2.0f - 1.0f;
			const float a = Rand01(s) * glm::two_pi<float>();
			const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
			return { r * std::cos(a), r * std::sin(a), z };
		}

		std::filesystem::path ResolveAsset(const std::string& stored) {
			std::filesystem::path p(stored);
			if (p.is_relative() && Project::GetActive()) p = Project::GetActiveAssetDirectory() / p;
			return p;
		}

		// Soft round sprite used when no texture is set
		Ref<Texture2D> DefaultSprite() {
			static Ref<Texture2D> s_Sprite = [] {
				constexpr uint32_t N = 64;
				std::vector<uint32_t> pixels(N * N);
				for (uint32_t y = 0; y < N; y++)
					for (uint32_t x = 0; x < N; x++) {
						const float dx = (x + 0.5f) / N * 2.0f - 1.0f, dy = (y + 0.5f) / N * 2.0f - 1.0f;
						float a = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
						a = a * a * (3.0f - 2.0f * a);
						pixels[y * N + x] = 0x00FFFFFFu | (static_cast<uint32_t>(a * 255.0f) << 24);
					}
				auto tex = Texture2D::Create(N, N);
				tex->SetData(pixels.data(), N * N * 4);
				return tex;
			}();
			return s_Sprite;
		}

		// (Re)load the assets referenced by the component when their paths change
		void ResolveResources(PSC& ps) {
			if (ps.LoadedTexturePath != ps.TexturePath) {
				ps.LoadedTexturePath = ps.TexturePath;
				ps.RuntimeTexture = ps.TexturePath.empty() ? nullptr : Texture2D::Create(ResolveAsset(ps.TexturePath).string());
				if (ps.RuntimeTexture && !ps.RuntimeTexture->IsLoaded()) ps.RuntimeTexture.reset();
			}
			if (ps.LoadedMeshPath != ps.MeshPath) {
				ps.LoadedMeshPath = ps.MeshPath;
				ps.RuntimeMesh.reset();
				if (!ps.MeshPath.empty()) {
					if (auto asset = MeshAsset::LoadFromFile(ResolveAsset(ps.MeshPath))) ps.RuntimeMesh = asset->GetModel();
				}
			}
			if (!ps.RuntimeMesh && ps.Mode == PSC::RenderMode::Mesh) {
				static Ref<Model> s_Cube = Model::CreateCube();
				ps.RuntimeMesh = s_Cube;
			}
			if (ps.LoadedMaterialPath != ps.MaterialPath) {
				ps.LoadedMaterialPath = ps.MaterialPath;
				ps.RuntimeMaterial = ps.MaterialPath.empty() ? nullptr : MaterialInstance::Create(ResolveAsset(ps.MaterialPath));
			}
			if (ps.LoadedPrefabPath != ps.PrefabPath) {
				ps.LoadedPrefabPath = ps.PrefabPath;
				ps.RuntimePrefab = ps.PrefabPath.empty() ? nullptr : std::static_pointer_cast<void>(Prefab::LoadFromFile(ResolveAsset(ps.PrefabPath)));
			}
		}

		glm::vec4 ColorAt(const PSC& ps, float t) { return glm::mix(ps.StartColor, ps.EndColor, t); }

		// Real entity for Physics / Prefab particles
		uint64_t SpawnEntity(Scene* scene, PSC& ps, const PSC::Particle& p) {
			Entity e;
			if (ps.Mode == PSC::RenderMode::Prefab && ps.RuntimePrefab) {
				auto prefab = std::static_pointer_cast<Prefab>(ps.RuntimePrefab);
				Ref<Scene> sceneRef(scene, [](Scene*) {});   // non-owning
				e = prefab->Instantiate(sceneRef, p.Position);
			}
			else {
				e = scene->CreateEntity("Particle");
				auto& tc = e.GetComponent<TransformComponent>();
				tc.Translation = p.Position;
				tc.Scale = glm::vec3(p.Size);
				auto& mc = e.AddComponent<MeshComponent>();
				mc.MeshModel = ps.RuntimeMesh;
				if (ps.RuntimeMaterial) e.AddOrReplaceComponent<MaterialComponent>().Instance = ps.RuntimeMaterial;
			}
			if (!e) return 0;

			if (ps.Simulation == PSC::SimulationMode::Physics && !e.HasComponent<Rigidbody3DComponent>()) {
				auto& rb = e.AddComponent<Rigidbody3DComponent>();
				rb.Mass = ps.ParticleMass;
				rb.Restitution = ps.Bounce;
				if (!e.HasComponent<SphereCollider3DComponent>() && !e.HasComponent<BoxCollider3DComponent>())
					e.AddComponent<BoxCollider3DComponent>();
			}
			return static_cast<uint64_t>(e.GetUUID());
		}

		void Spawn(Scene* scene, PSC& ps, const glm::mat4& emitter, bool spawnEntities) {
			if (static_cast<int>(ps.Particles.size()) >= ps.MaxParticles) return;
			uint32_t& s = ps.RandomState;

			// Position + direction in the emitter's local space
			glm::vec3 local(0.0f), dir(0.0f, 1.0f, 0.0f);
			switch (ps.Shape) {
				case PSC::EmitterShape::Sphere: {
					dir = RandomUnitVector(s);
					local = dir * ps.ShapeRadius * std::cbrt(Rand01(s));
					break;
				}
				case PSC::EmitterShape::Box: {
					local = (glm::vec3(Rand01(s), Rand01(s), Rand01(s)) - 0.5f) * ps.BoxSize;
					break;
				}
				case PSC::EmitterShape::Cone: {
					const float angle = glm::radians(ps.ConeAngle) * std::sqrt(Rand01(s));
					const float az = Rand01(s) * glm::two_pi<float>();
					dir = glm::vec3(std::sin(angle) * std::cos(az), std::cos(angle), std::sin(angle) * std::sin(az));
					const float r = ps.ShapeRadius * std::sqrt(Rand01(s));
					local = glm::vec3(r * std::cos(az), 0.0f, r * std::sin(az));
					break;
				}
				default: {
					dir = RandomUnitVector(s);
					if (dir.y < 0.0f) dir.y = -dir.y;
					break;
				}
			}

			PSC::Particle p;
			const float speed = RandRange(s, ps.Speed);
			if (ps.WorldSpace) {
				p.Position = glm::vec3(emitter * glm::vec4(local, 1.0f));
				p.Velocity = glm::normalize(glm::mat3(emitter) * dir) * speed;
			}
			else {
				p.Position = local;
				p.Velocity = dir * speed;
			}
			p.Life = std::max(0.05f, RandRange(s, ps.Lifetime));
			p.Size = RandRange(s, ps.StartSize);
			p.AngularSpeed = glm::radians(RandRange(s, ps.AngularSpeed));
			p.Rotation = Rand01(s) * glm::two_pi<float>();
			p.Color = ps.StartColor;
			if (spawnEntities) p.Entity = SpawnEntity(scene, ps, p);
			ps.Particles.push_back(p);
		}

		glm::mat4 ParticleTransform(const PSC& ps, const PSC::Particle& p, const glm::mat4& emitter, float size) {
			glm::mat4 m = glm::translate(glm::mat4(1.0f), p.Position) *
				glm::rotate(glm::mat4(1.0f), p.Rotation, glm::vec3(0.3f, 1.0f, 0.2f)) *
				glm::scale(glm::mat4(1.0f), glm::vec3(size));
			return ps.WorldSpace ? m : emitter * m;
		}
	}

	// ============================================================================
	// LIFECYCLE
	// ============================================================================

	void ParticleSystem::OnRuntimeStart(SceneMode mode) {
		if (!m_Context || !m_Context->Registry) return;
		auto view = m_Context->Registry->view<PSC>();
		for (auto e : view) {
			auto& ps = view.get<PSC>(e);
			ps.Particles.clear();
			ps.PendingEmit = 0;
			ps.EmitAccumulator = 0.0f;
			ps.Playing = ps.PlayOnStart;
			ps.CycleTime = 0.0f;
			ps.BurstPending = true;
			ps.RandomState = 0x9E3779B9u ^ static_cast<uint32_t>(e);
		}
	}

	void ParticleSystem::OnRuntimeStop() {
		if (!m_Context || !m_Context->Registry) return;
		auto view = m_Context->Registry->view<PSC>();
		for (auto e : view) view.get<PSC>(e).Particles.clear();
	}

	// ============================================================================
	// SIMULATION
	// ============================================================================

	void ParticleSystem::OnUpdate(Timestep ts, SceneMode mode) {
		if (!m_Context || !m_Context->Registry || !m_Context->OwningScene) return;
		Scene* scene = m_Context->OwningScene;
		const bool runtime = mode == SceneMode::Play || mode == SceneMode::Simulate;
		const float dt = std::min(static_cast<float>(ts), 0.1f);

		auto view = m_Context->Registry->view<PSC>();
		for (auto handle : view) {
			auto& ps = view.get<PSC>(handle);
			Entity entity{ handle, scene };

			if (!runtime) {
				// Editor preview: always the cheap simulation, never spawns entities
				if (!ps.PreviewInEditor) { ps.Particles.clear(); continue; }
				if (!ps.Playing) { ps.Playing = true; ps.CycleTime = 0.0f; ps.BurstPending = true; }
			}
			ResolveResources(ps);

			const glm::mat4 emitter = scene->GetWorldTransform(entity);
			const bool spawnEntities = runtime &&
				(ps.Simulation == PSC::SimulationMode::Physics || ps.Mode == PSC::RenderMode::Prefab);

			// ---- Emission ----
			int toEmit = ps.PendingEmit;
			ps.PendingEmit = 0;
			if (ps.Playing) {
				if (ps.BurstPending) { toEmit += ps.Burst; ps.BurstPending = false; }
				ps.EmitAccumulator += ps.Rate * dt;
				const int n = static_cast<int>(ps.EmitAccumulator);
				ps.EmitAccumulator -= static_cast<float>(n);
				toEmit += n;
				ps.CycleTime += dt;
				if (ps.CycleTime >= ps.Duration) {
					if (ps.Looping) { ps.CycleTime -= ps.Duration; ps.BurstPending = true; }
					else ps.Playing = false;
				}
			}
			for (int i = 0; i < toEmit; i++) Spawn(scene, ps, emitter, spawnEntities);

			// ---- Integrate / age ----
			const glm::vec3 gravity(0.0f, -9.81f * ps.GravityScale, 0.0f);
			for (auto& p : ps.Particles) {
				p.Age += dt;
				const float t = std::clamp(p.Age / p.Life, 0.0f, 1.0f);
				p.Color = ColorAt(ps, t);

				if (p.Entity) {
					// Bullet / prefab drives the motion; follow the entity for the visuals
					Entity e = scene->GetEntityByUUID(UUID(p.Entity));
					if (!e) { p.Entity = 0; p.Age = p.Life; continue; }
					auto& tc = e.GetComponent<TransformComponent>();
					p.Position = tc.Translation;
					if (e.HasComponent<Rigidbody3DComponent>()) {
						auto& rb = e.GetComponent<Rigidbody3DComponent>();
						// First frame with a body: give it the emission velocity
						if (rb.RuntimeBody && p.Velocity != glm::vec3(0.0f)) {
							static_cast<RigidBodyComponent*>(rb.RuntimeBody)->SetLinearVelocity(p.Velocity);
							p.Velocity = glm::vec3(0.0f);
						}
					}
					continue;
				}

				p.Velocity += gravity * dt;
				p.Velocity *= std::max(0.0f, 1.0f - ps.Drag * dt);
				p.Position += p.Velocity * dt;
				p.Rotation += p.AngularSpeed * dt;
				if (ps.CollideWithGround) {
					const float ground = ps.WorldSpace ? ps.GroundHeight : ps.GroundHeight - emitter[3].y;
					if (p.Position.y < ground) {
						p.Position.y = ground;
						if (p.Velocity.y < 0.0f) p.Velocity.y = -p.Velocity.y * ps.Bounce;
						p.Velocity.x *= 0.8f; p.Velocity.z *= 0.8f;
					}
				}
			}

			// ---- Remove dead particles (and their entities) ----
			for (size_t i = 0; i < ps.Particles.size();) {
				auto& p = ps.Particles[i];
				if (p.Age < p.Life) { i++; continue; }
				if (p.Entity) {
					Entity e = scene->GetEntityByUUID(UUID(p.Entity));
					if (e) scene->DestroyEntity(e);
				}
				p = ps.Particles.back();
				ps.Particles.pop_back();
			}
		}
	}

	// ============================================================================
	// RENDERING
	// ============================================================================

	void ParticleSystem::SubmitMeshParticles(Scene* scene, entt::registry& registry, bool deferred) {
		static MeshComponent s_Mesh;
		static MaterialComponent s_Material;

		auto view = registry.view<PSC>();
		for (auto handle : view) {
			auto& ps = view.get<PSC>(handle);
			if (ps.Mode != PSC::RenderMode::Mesh || ps.Particles.empty() || !ps.RuntimeMesh) continue;
			const glm::mat4 emitter = scene->GetWorldTransform(Entity{ handle, scene });
			s_Mesh.MeshModel = ps.RuntimeMesh;
			s_Material.Instance = ps.RuntimeMaterial;
			const int id = static_cast<int>(handle);

			for (const auto& p : ps.Particles) {
				if (p.Entity) continue;   // drawn by its own entity
				const float t = std::clamp(p.Age / p.Life, 0.0f, 1.0f);
				const glm::mat4 m = ParticleTransform(ps, p, emitter, p.Size * glm::mix(1.0f, ps.EndSizeScale, t));
				if (deferred) {
					if (s_Material.Instance) DeferredRenderer::SubmitMesh(m, s_Mesh, s_Material, id);
					else DeferredRenderer::SubmitMesh(m, s_Mesh, id);
				}
				else {
					if (s_Material.Instance) Renderer3D::DrawMesh(m, s_Mesh, s_Material, id);
					else Renderer3D::DrawMesh(m, s_Mesh, id);
				}
			}
		}
		s_Mesh.MeshModel.reset();
		s_Material.Instance.reset();
	}

	void ParticleSystem::DrawBillboards(Scene* scene, entt::registry& registry, const glm::vec3& cameraPosition) {
		auto view = registry.view<PSC>();
		for (auto handle : view) {
			auto& ps = view.get<PSC>(handle);
			// Prefab particles are visible by themselves; in the editor preview they show as sprites
			const bool sprites = ps.Mode == PSC::RenderMode::Billboard ||
				(ps.Mode == PSC::RenderMode::Prefab && !ps.Particles.empty() && ps.Particles.front().Entity == 0);
			if (!sprites || ps.Particles.empty()) continue;

			Ref<Texture2D> texture = ps.RuntimeTexture ? ps.RuntimeTexture : DefaultSprite();
			Renderer2D::SetParticleBlending(ps.AdditiveBlend, false);
			const glm::mat4 emitter = ps.WorldSpace ? glm::mat4(1.0f) : scene->GetWorldTransform(Entity{ handle, scene });
			for (const auto& p : ps.Particles) {
				const float t = std::clamp(p.Age / p.Life, 0.0f, 1.0f);
				const float size = p.Size * glm::mix(1.0f, ps.EndSizeScale, t);
				const glm::vec3 pos = ps.WorldSpace ? p.Position : glm::vec3(emitter * glm::vec4(p.Position, 1.0f));

				// Camera-facing quad with roll, tinted by the particle color
				const glm::vec3 toCamera = glm::normalize(cameraPosition - pos);
				glm::vec3 right = glm::cross(glm::vec3(0, 1, 0), toCamera);
				right = glm::length(right) < 1e-4f ? glm::vec3(1, 0, 0) : glm::normalize(right);
				const glm::vec3 up = glm::cross(toCamera, right);
				const float c = std::cos(p.Rotation), s = std::sin(p.Rotation);
				const glm::vec3 r2 = right * c + up * s, u2 = -right * s + up * c;
				glm::mat4 m(1.0f);
				m[0] = glm::vec4(r2 * size, 0.0f);
				m[1] = glm::vec4(u2 * size, 0.0f);
				m[2] = glm::vec4(toCamera, 0.0f);
				m[3] = glm::vec4(pos, 1.0f);
				Renderer2D::DrawQuad(m, texture, 1.0f, p.Color, static_cast<int>(handle));
			}
		}
		Renderer2D::SetParticleBlending(false, true);
	}

} // namespace Lunex
