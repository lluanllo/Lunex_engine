#include "stpch.h"
#include "SceneRenderSystem.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/GridRenderer.h"
#include "Renderer/SkyboxRenderer.h"
#include "RHI/RHI.h"

#include <algorithm>

namespace Lunex {

	// ========================================================================
	// ISceneSystem Interface
	// ========================================================================

	void SceneRenderSystem::OnAttach(SceneContext& context) {
		m_Context = &context;

		// Create both backends
		m_RasterBackend  = CreateScope<RasterBackend>();
		m_RTBackend      = CreateScope<RayTracingBackend>();
		m_RasterBackend->Initialize();
		m_RTBackend->Initialize();

		// Default to raster
		m_ActiveBackendType = RenderBackendType::Rasterizer;
		m_ActiveBackend     = m_RasterBackend.get();

		if (m_Context->OwningScene) {
			m_RasterBackend->OnSceneChanged(m_Context->OwningScene);
			m_RTBackend->OnSceneChanged(m_Context->OwningScene);
		}

		LNX_LOG_INFO("SceneRenderSystem attached (dual backend)");
	}

	void SceneRenderSystem::OnDetach() {
		m_RTBackend->Shutdown();
		m_RasterBackend->Shutdown();
		m_ActiveBackend = nullptr;
		m_Context = nullptr;
		LNX_LOG_INFO("SceneRenderSystem detached");
	}

	void SceneRenderSystem::OnUpdate(Timestep ts, SceneMode mode) {
		// Actual rendering is done via RenderScene/RenderSceneRuntime
	}

	void SceneRenderSystem::OnSceneEvent(const SceneSystemEvent& event) {
		if (event.Type == SceneEventType::ViewportResized) {
			// Could update render targets here if needed
		}
	}

	// ========================================================================
	// Backend Switching
	// ========================================================================

	void SceneRenderSystem::SetActiveBackend(RenderBackendType type) {
		if (type == m_ActiveBackendType) return;

		m_ActiveBackendType = type;
		switch (type) {
			case RenderBackendType::Rasterizer:
				m_ActiveBackend = m_RasterBackend.get();
				break;
			case RenderBackendType::PathTracer:
				m_ActiveBackend = m_RTBackend.get();
				break;
		}

		// Notify new backend about the current scene
		if (m_Context && m_Context->OwningScene) {
			m_ActiveBackend->OnSceneChanged(m_Context->OwningScene);
		}

		LNX_LOG_INFO("Render backend switched to: {0}",
		             RenderBackendTypeToString(type));
	}

	void SceneRenderSystem::NotifySceneChanged() {
		if (m_Context && m_Context->OwningScene) {
			m_RasterBackend->OnSceneChanged(m_Context->OwningScene);
			m_RTBackend->OnSceneChanged(m_Context->OwningScene);
		}
	}

	void SceneRenderSystem::NotifyMaterialChanged() {
		// Material property changes affect the path tracer image;
		// the BVH is still valid so we only need to reset accumulation
		// (the scene hash will pick up the texture/value change next frame).
		if (m_RTBackend) {
			m_RTBackend->ResetAccumulation();
		}
	}

	void SceneRenderSystem::OnViewportResize(uint32_t w, uint32_t h) {
		m_RTBackend->OnViewportResize(w, h);
		// Raster doesn't own textures, no-op
	}

	// ========================================================================
	// Render API
	// ========================================================================

	void SceneRenderSystem::RenderScene(EditorCamera& camera) {
		if (!m_Context || !m_Context->Registry) return;

		bool isPathTracer = (m_ActiveBackendType == RenderBackendType::PathTracer);

		// ========== 3D RENDERING — delegated to active backend ==========
		// Run FIRST so the path tracer compute shader finishes before we
		// need its output, and the rasterizer draws into the framebuffer
		// before 2D overlays go on top.
		m_ActiveBackend->BeginFrame(camera);
		m_ActiveBackend->RenderScene(m_Context->OwningScene);
		m_ActiveBackend->EndFrame();

		// ========== PATH TRACER COMPOSITING ==========
		// When the path tracer is active its output lives in a standalone
		// texture.  Blit it into the currently-bound editor framebuffer so
		// that all subsequent raster passes (skybox, grid, billboards,
		// gizmos, selection outline) are drawn ON TOP.
		if (isPathTracer) {
			auto* rtBackend = static_cast<RayTracingBackend*>(m_ActiveBackend);
			rtBackend->BlitToFramebuffer();
		}

		// ========== SKYBOX (always raster) ==========
		// When using the path tracer the skybox is already baked into the
		// traced image, so skip the raster skybox to avoid double-draws.
		if (m_Settings.RenderSkybox && !isPathTracer) {
			SkyboxRenderer::RenderGlobalSkybox(camera);
		}

		// ========== 2D RENDERING (always raster) ==========
		Renderer2D::BeginScene(camera);

		if (m_Settings.RenderGrid) {
			GridRenderer::DrawGrid(camera);
		}

		RenderSprites(camera, camera.GetViewMatrix());
		RenderCircles(camera, camera.GetViewMatrix());

		Renderer2D::EndScene();

		// ========== BILLBOARDS & GIZMOS (always raster) ==========
		if (m_Settings.RenderBillboards || m_Settings.RenderGizmos) {
			Renderer2D::BeginScene(camera);

			if (m_Settings.RenderBillboards) {
				RenderBillboards(camera, camera.GetPosition());
			}

			if (m_Settings.RenderGizmos) {
				float previousLineWidth = Renderer2D::GetLineWidth();
				Renderer2D::SetLineWidth(m_Settings.BillboardLineWidth);

				if (m_Settings.RenderFrustums) {
					RenderCameraFrustums(camera);
				}

				RenderLightGizmos(camera);

				Renderer2D::SetLineWidth(previousLineWidth);
			}

			Renderer2D::EndScene();
		}
	}

	void SceneRenderSystem::RenderSceneRuntime(Camera& camera, const glm::mat4& cameraTransform) {
		if (!m_Context || !m_Context->Registry) return;

		bool isPathTracer = (m_ActiveBackendType == RenderBackendType::PathTracer);

		// ========== 3D RENDERING — delegated to active backend ==========
		m_ActiveBackend->BeginFrameRuntime(camera, cameraTransform);
		m_ActiveBackend->RenderScene(m_Context->OwningScene);
		m_ActiveBackend->EndFrame();

		// ========== PATH TRACER COMPOSITING ==========
		if (isPathTracer) {
			auto* rtBackend = static_cast<RayTracingBackend*>(m_ActiveBackend);
			rtBackend->BlitToFramebuffer();
		}

		// ========== SKYBOX (always raster) ==========
		if (m_Settings.RenderSkybox && !isPathTracer) {
			SkyboxRenderer::RenderGlobalSkybox(camera, cameraTransform);
		}

		// ========== 2D RENDERING (always raster) ==========
		Renderer2D::BeginScene(camera, cameraTransform);

		RenderSprites(camera, cameraTransform);
		RenderCircles(camera, cameraTransform);

		Renderer2D::EndScene();
	}

	// ========================================================================
	// Render Passes (unchanged)
	// ========================================================================

	void SceneRenderSystem::RenderSkybox(const Camera& camera, const glm::mat4& cameraTransform) {
		SkyboxRenderer::RenderGlobalSkybox(camera, cameraTransform);
	}

	void SceneRenderSystem::RenderGrid(EditorCamera& camera) {
		GridRenderer::DrawGrid(camera);
	}

	void SceneRenderSystem::RenderSprites(const Camera& camera, const glm::mat4& cameraTransform) {
		auto group = m_Context->Registry->group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group) {
			auto& sprite = group.get<SpriteRendererComponent>(entity);
			glm::mat4 worldTransform = GetWorldTransform(entity);
			Renderer2D::DrawSprite(worldTransform, sprite, static_cast<int>(entity));
		}
	}

	void SceneRenderSystem::RenderCircles(const Camera& camera, const glm::mat4& cameraTransform) {
		auto view = m_Context->Registry->view<TransformComponent, CircleRendererComponent>();
		for (auto entity : view) {
			auto& circle = view.get<CircleRendererComponent>(entity);
			glm::mat4 worldTransform = GetWorldTransform(entity);
			Renderer2D::DrawCircle(worldTransform, circle.Color, circle.Thickness, circle.Fade, static_cast<int>(entity));
		}
	}

	void SceneRenderSystem::RenderMeshes(const Camera& camera, const glm::mat4& cameraTransform) {
		auto view = m_Context->Registry->view<TransformComponent, MeshComponent>();
		for (auto entity : view) {
			Entity e = { entity, m_Context->OwningScene };
			auto& mesh = view.get<MeshComponent>(entity);
			glm::mat4 worldTransform = GetWorldTransform(entity);
			
			if (e.HasComponent<MaterialComponent>()) {
				auto& material = e.GetComponent<MaterialComponent>();
				Renderer3D::DrawMesh(worldTransform, mesh, material, static_cast<int>(entity));
			}
			else {
				Renderer3D::DrawModel(worldTransform, mesh.MeshModel, mesh.Color, static_cast<int>(entity));
			}
		}
	}

	void SceneRenderSystem::RenderBillboards(const Camera& camera, const glm::vec3& cameraPosition) {
		struct BillboardData {
			glm::vec3 Position;
			Ref<Texture2D> Texture;
			int EntityID;
			float Distance;
			float Size;
			int Priority;
		};
		
		std::vector<BillboardData> billboards;
		
		// Collect light billboards
		{
			auto view = m_Context->Registry->view<TransformComponent, LightComponent>();
			for (auto entity : view) {
				auto& light = view.get<LightComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(entity);
				glm::vec3 worldPos = glm::vec3(worldTransform[3]);
				
				if (light.IconTexture && light.IconTexture->IsLoaded()) {
					float distance = glm::length(cameraPosition - worldPos);
					billboards.push_back({ worldPos, light.IconTexture, static_cast<int>(entity), distance, 0.5f, 0 });
				}
			}
		}
		
		// Collect camera billboards
		{
			auto view = m_Context->Registry->view<TransformComponent, CameraComponent>();
			for (auto entity : view) {
				auto& cameraComp = view.get<CameraComponent>(entity);
				glm::mat4 worldTransform = GetWorldTransform(entity);
				glm::vec3 worldPos = glm::vec3(worldTransform[3]);
				
				if (cameraComp.IconTexture && cameraComp.IconTexture->IsLoaded()) {
					float distance = glm::length(cameraPosition - worldPos);
					glm::vec3 toCamera = glm::normalize(cameraPosition - worldPos);
					glm::vec3 offsetPos = worldPos + toCamera * 0.1f;
					billboards.push_back({ offsetPos, cameraComp.IconTexture, static_cast<int>(entity), distance, 1.0f, 1 });
				}
			}
		}
		
		// Sort by distance (back to front)
		std::sort(billboards.begin(), billboards.end(),
			[](const BillboardData& a, const BillboardData& b) {
				if (std::abs(a.Distance - b.Distance) < 0.01f) {
					return a.Priority < b.Priority;
				}
				return a.Distance > b.Distance;
			});
		
		// Render billboards
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetDepthMask(false);
		}
		
		for (const auto& billboard : billboards) {
			Renderer2D::DrawBillboard(billboard.Position, billboard.Texture,
				cameraPosition, billboard.Size, billboard.EntityID);
		}
		
		if (cmdList) {
			cmdList->SetDepthMask(true);
		}
	}

	void SceneRenderSystem::RenderCameraFrustums(const Camera& camera) {
		auto view = m_Context->Registry->view<TransformComponent, CameraComponent>();
		for (auto entity : view) {
			auto& cameraComp = view.get<CameraComponent>(entity);
			glm::mat4 worldTransform = GetWorldTransform(entity);
			
			glm::mat4 cameraProjection = cameraComp.Camera.GetProjection();
			glm::mat4 cameraView = glm::inverse(worldTransform);
			
			glm::vec4 frustumColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			Renderer2D::DrawCameraFrustum(cameraProjection, cameraView, frustumColor, static_cast<int>(entity));
		}
	}

	void SceneRenderSystem::RenderLightGizmos(const Camera& camera) {
		auto view = m_Context->Registry->view<TransformComponent, LightComponent>();
		for (auto entity : view) {
			auto& light = view.get<LightComponent>(entity);
			glm::mat4 worldTransform = GetWorldTransform(entity);
			glm::vec3 worldPos = glm::vec3(worldTransform[3]);
			
			glm::vec4 gizmoColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			glm::vec3 forward = -glm::normalize(glm::vec3(worldTransform[2]));
			
			switch (light.GetType()) {
				case LightType::Point:
					Renderer2D::DrawPointLightGizmo(worldPos, light.GetRange(), gizmoColor, static_cast<int>(entity));
					break;
				case LightType::Directional:
					Renderer2D::DrawDirectionalLightGizmo(worldPos, forward, gizmoColor, static_cast<int>(entity));
					break;
				case LightType::Spot:
					Renderer2D::DrawSpotLightGizmo(worldPos, forward, light.GetRange(), light.GetOuterConeAngle(), gizmoColor, static_cast<int>(entity));
					break;
			}
		}
	}

	// ========================================================================
	// Helpers
	// ========================================================================

	glm::mat4 SceneRenderSystem::GetWorldTransform(entt::entity entity) {
		if (!m_Context || !m_Context->OwningScene) {
			return glm::mat4(1.0f);
		}
		
		Entity e = { entity, m_Context->OwningScene };
		return m_Context->OwningScene->GetWorldTransform(e);
	}

} // namespace Lunex
