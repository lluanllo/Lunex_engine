#include "stpch.h"
#include "RasterBackend.h"

#include "Renderer/Renderer3D.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Shadows/ShadowSystem.h"
#include "Scene/Lighting/LightSystem.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Log/Log.h"

#include <glad/glad.h>

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

		// Ensure clean GL state for the raster pipeline.
		// When switching from the path tracer backend, compute dispatch and
		// the entity-ID raster pass may leave depth, blend, cull, or color-mask
		// state in an unexpected configuration.
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glDisable(GL_BLEND);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		// Ensure all draw buffer color masks are enabled (the PT entity ID
		// pass disables color writes on attachment 0 via glColorMaski)
		glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// Unbind any active compute/program from the path tracer
		glUseProgram(0);

		// Lights must be synced BEFORE BeginScene.
		// NOTE: Shadows are rendered by the EditorLayer BEFORE binding the main
		// framebuffer, so we do NOT call UpdateShadows here (it would corrupt
		// the currently-bound FBO).
		if (m_CurrentScene) {
			LightSystem::Get().SyncFromScene(m_CurrentScene);
			Renderer3D::UpdateLights(m_CurrentScene);
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

		// Ensure clean GL state (same as BeginFrame)
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glDisable(GL_BLEND);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glUseProgram(0);

		if (m_CurrentScene) {
			LightSystem::Get().SyncFromScene(m_CurrentScene);
			Renderer3D::UpdateLights(m_CurrentScene);
		}

		Renderer3D::BeginScene(camera, cameraTransform);
	}

	// ====================================================================
	// RENDER — identical to SceneRenderSystem::RenderMeshes
	// ====================================================================

	void RasterBackend::RenderScene(Scene* scene) {
		if (!scene) return;

		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();

		for (auto entity : view) {
			Entity e = { entity, scene };
			auto& mesh = view.get<MeshComponent>(entity);
			glm::mat4 worldTransform = scene->GetWorldTransform(e);

			if (e.HasComponent<MaterialComponent>()) {
				auto& material = e.GetComponent<MaterialComponent>();
				Renderer3D::DrawMesh(worldTransform, mesh, material,
				                     static_cast<int>(entity));
			} else {
				Renderer3D::DrawMesh(worldTransform, mesh,
				                     static_cast<int>(entity));
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
