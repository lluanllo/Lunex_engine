#include "stpch.h"
#include "RasterBackend.h"

#include "Renderer/Renderer3D.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Shadows/ShadowSystem.h"
#include "Scene/Lighting/LightSystem.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Log/Log.h"

namespace Lunex {

	// ====================================================================
	// LIFECYCLE
	// ====================================================================

	void RasterBackend::Initialize() {
		LNX_LOG_INFO("RasterBackend: Initialized (wrapping Renderer3D)");
	}

	void RasterBackend::Shutdown() {
		LNX_LOG_INFO("RasterBackend: Shutdown");
	}

	// ====================================================================
	// EDITOR
	// ====================================================================

	void RasterBackend::BeginFrame(const EditorCamera& camera) {
		m_EditorMode   = true;
		m_EditorCamera = &camera;

		// Lights + shadows must run BEFORE BeginScene (unchanged flow)
		if (m_CurrentScene) {
			LightSystem::Get().SyncFromScene(m_CurrentScene);
			Renderer3D::UpdateLights(m_CurrentScene);
			Renderer3D::UpdateShadows(m_CurrentScene, camera);
		}

		Renderer3D::BeginScene(camera);
	}

	// ====================================================================
	// RUNTIME
	// ====================================================================

	void RasterBackend::BeginFrameRuntime(const Camera& camera,
	                                       const glm::mat4& cameraTransform) {
		m_EditorMode               = false;
		m_RuntimeCamera            = &camera;
		m_RuntimeCameraTransform   = cameraTransform;

		if (m_CurrentScene) {
			LightSystem::Get().SyncFromScene(m_CurrentScene);
			Renderer3D::UpdateLights(m_CurrentScene);
			Renderer3D::UpdateShadows(m_CurrentScene, camera, cameraTransform);
		}

		Renderer3D::BeginScene(camera, cameraTransform);
	}

	// ====================================================================
	// RENDER — identical to SceneRenderSystem::RenderMeshes
	// ====================================================================

	void RasterBackend::RenderScene(Scene* scene) {
		if (!scene) return;

		auto& registry = *scene->GetRegistry();
		auto view = registry.view<TransformComponent, MeshComponent>();

		for (auto entity : view) {
			Entity e = { entity, scene };
			auto& mesh = view.get<MeshComponent>(entity);
			glm::mat4 worldTransform = scene->GetWorldTransform(e);

			if (e.HasComponent<MaterialComponent>()) {
				auto& material = e.GetComponent<MaterialComponent>();
				Renderer3D::DrawMesh(worldTransform, mesh, material,
				                     static_cast<int>(entity));
			} else {
				Renderer3D::DrawModel(worldTransform, mesh.MeshModel,
				                      mesh.Color, static_cast<int>(entity));
			}
		}
	}

	void RasterBackend::EndFrame() {
		Renderer3D::EndScene();
	}

	// ====================================================================
	// NOTIFICATIONS
	// ====================================================================

	void RasterBackend::OnSceneChanged(Scene* scene) {
		m_CurrentScene = scene;
	}

	void RasterBackend::OnViewportResize(uint32_t /*w*/, uint32_t /*h*/) {
		// Raster path doesn't own any textures — nothing to resize.
	}

	// ====================================================================
	// STATS
	// ====================================================================

	RenderBackendStats RasterBackend::GetStats() const {
		auto s = Renderer3D::GetStats();
		RenderBackendStats out;
		out.DrawCalls     = s.DrawCalls;
		out.TriangleCount = s.TriangleCount;
		out.MeshCount     = s.MeshCount;
		return out;
	}

} // namespace Lunex
