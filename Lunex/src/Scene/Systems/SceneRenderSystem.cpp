#include "stpch.h"
#include "SceneRenderSystem.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/Deferred/DeferredRenderer.h"
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
		LNX_LOG_INFO("SceneRenderSystem attached");
	}

	void SceneRenderSystem::OnDetach() {
		m_Context = nullptr;
		LNX_LOG_INFO("SceneRenderSystem detached");
	}

	void SceneRenderSystem::OnUpdate(Timestep ts, SceneMode mode) {
		// Actual rendering is done via RenderScene/RenderSceneRuntime
		// This update is for any per-frame state updates
	}

	void SceneRenderSystem::OnSceneEvent(const SceneSystemEvent& event) {
		// Handle viewport resize
		if (event.Type == SceneEventType::ViewportResized) {
			// Could update render targets here if needed
		}
	}

	// ========================================================================
	// Render API
	// ========================================================================

	void SceneRenderSystem::RenderScene(EditorCamera& camera) {
		if (!m_Context || !m_Context->Registry) return;
		
		// ========== 3D RENDERING ==========
		// Update lights first (syncs LightSystem, uploads SSBO)
		Renderer3D::UpdateLights(m_Context->OwningScene);
		
		// Update shadow maps BEFORE BeginScene (BeginScene binds atlas for reading)
		Renderer3D::UpdateShadows(m_Context->OwningScene, camera);
		
		if (DeferredRenderer::IsEnabled()) {
			// ========================================
			// DEFERRED PATH: Only geometry pass here.
			// Skybox, grid, sprites, billboards, gizmos render AFTER
			// the lighting pass via RenderPostLighting().
			// ========================================
			DeferredRenderer::BeginScene(camera);
			DeferredRenderer::BeginGeometryPass();
			RenderMeshes(camera, camera.GetViewMatrix());
			DeferredRenderer::EndGeometryPass();
			DeferredRenderer::EndScene();
			// Lighting pass + post-lighting overlays are done by caller (EditorLayer)
		} else {
			// Forward path: Skybox first, then grid, then meshes, then overlays
			if (m_Settings.RenderSkybox) {
				SkyboxRenderer::RenderGlobalSkybox(camera);
			}
			
			Renderer2D::BeginScene(camera);
			if (m_Settings.RenderGrid) {
				GridRenderer::DrawGrid(camera);
			}
			RenderSprites(camera, camera.GetViewMatrix());
			RenderCircles(camera, camera.GetViewMatrix());
			Renderer2D::EndScene();
			
			Renderer3D::BeginScene(camera);
			RenderMeshes(camera, camera.GetViewMatrix());
			Renderer3D::EndScene();
			
			// Billboards & Gizmos (forward path renders them here)
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
	}

	void SceneRenderSystem::RenderPostLighting(EditorCamera& camera) {
		if (!m_Context || !m_Context->Registry) return;
		if (!DeferredRenderer::IsEnabled()) return; // Only needed for deferred path
		
		// ========================================
		// POST-LIGHTING SCENE ELEMENTS (Deferred path only)
		// Called AFTER DeferredRenderer::ExecuteLightingPass() + depth blit.
		// These elements are rendered BEFORE post-processing so bloom can sample them.
		// ========================================
		
		// Skybox renders with depth func LEQUAL, writes at far plane
		// Skybox IS affected by bloom (correct: bright sky areas should glow)
		if (m_Settings.RenderSkybox) {
			SkyboxRenderer::RenderGlobalSkybox(camera);
		}
		
		// Sprites that are part of the scene should also be affected by bloom
		Renderer2D::BeginScene(camera);
		RenderSprites(camera, camera.GetViewMatrix());
		RenderCircles(camera, camera.GetViewMatrix());
		Renderer2D::EndScene();
	}

	void SceneRenderSystem::RenderPostLightingOverlays(EditorCamera& camera) {
		if (!m_Context || !m_Context->Registry) return;
		if (!DeferredRenderer::IsEnabled()) return;
		
		// ========================================
		// EDITOR OVERLAYS (Deferred path only)
		// Called AFTER post-processing (bloom + tone mapping).
		// These elements are NOT affected by bloom.
		// ========================================
		
		// Grid renders here so it's not affected by bloom/tone mapping
		Renderer2D::BeginScene(camera);
		if (m_Settings.RenderGrid) {
			GridRenderer::DrawGrid(camera);
		}
		Renderer2D::EndScene();
		
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

	void SceneRenderSystem::RenderPostLightingRuntime(Camera& camera, const glm::mat4& cameraTransform) {
		if (!m_Context || !m_Context->Registry) return;
		if (!DeferredRenderer::IsEnabled()) return;
		
		// Skybox (affected by bloom)
		if (m_Settings.RenderSkybox) {
			SkyboxRenderer::RenderGlobalSkybox(camera, cameraTransform);
		}
		
		// 2D sprites (affected by bloom)
		Renderer2D::BeginScene(camera, cameraTransform);
		RenderSprites(camera, cameraTransform);
		RenderCircles(camera, cameraTransform);
		Renderer2D::EndScene();
	}

	void SceneRenderSystem::RenderSceneRuntime(Camera& camera, const glm::mat4& cameraTransform) {
		if (!m_Context || !m_Context->Registry) return;
		
		// ========== 3D RENDERING ==========
		Renderer3D::UpdateLights(m_Context->OwningScene);
		
		// Update shadow maps BEFORE BeginScene (BeginScene binds atlas for reading)
		Renderer3D::UpdateShadows(m_Context->OwningScene, camera, cameraTransform);
		
		if (DeferredRenderer::IsEnabled()) {
			// Deferred path: only geometry pass here
			DeferredRenderer::BeginScene(camera, cameraTransform);
			DeferredRenderer::BeginGeometryPass();
			RenderMeshes(camera, cameraTransform);
			DeferredRenderer::EndGeometryPass();
			DeferredRenderer::EndScene();
			// Lighting pass + post-lighting overlays done by caller
		} else {
			// Forward path
			if (m_Settings.RenderSkybox) {
				SkyboxRenderer::RenderGlobalSkybox(camera, cameraTransform);
			}
			
			Renderer2D::BeginScene(camera, cameraTransform);
			RenderSprites(camera, cameraTransform);
			RenderCircles(camera, cameraTransform);
			Renderer2D::EndScene();
			
			Renderer3D::UpdateShadows(m_Context->OwningScene, camera, cameraTransform);
			
			Renderer3D::BeginScene(camera, cameraTransform);
			RenderMeshes(camera, cameraTransform);
			Renderer3D::EndScene();
		}
	}

	// ========================================================================
	// Render Passes
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
			
			if (DeferredRenderer::IsEnabled()) {
				// Deferred path: submit to DeferredRenderer
				if (e.HasComponent<MaterialComponent>()) {
					auto& material = e.GetComponent<MaterialComponent>();
					DeferredRenderer::SubmitMesh(worldTransform, mesh, material, static_cast<int>(entity));
				} else {
					DeferredRenderer::SubmitMesh(worldTransform, mesh, static_cast<int>(entity));
				}
			} else {
				// Forward path: submit to Renderer3D
				if (e.HasComponent<MaterialComponent>()) {
					auto& material = e.GetComponent<MaterialComponent>();
					Renderer3D::DrawMesh(worldTransform, mesh, material, static_cast<int>(entity));
				} else {
					Renderer3D::DrawModel(worldTransform, mesh.MeshModel, mesh.Color, static_cast<int>(entity));
				}
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
		
		// Render billboards - disable both depth test and depth write
		// Billboards should always render on top of scene geometry
		// Back-to-front sorting handles billboard-to-billboard ordering
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetDepthTestEnabled(false);
			cmdList->SetDepthMask(false);
		}
		
		for (const auto& billboard : billboards) {
			Renderer2D::DrawBillboard(billboard.Position, billboard.Texture,
				cameraPosition, billboard.Size, billboard.EntityID);
		}
		
		if (cmdList) {
			cmdList->SetDepthTestEnabled(true);
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
