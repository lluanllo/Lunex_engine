#include "stpch.h"
#include "RenderSystem.h"
#include "Log/Log.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/GridRenderer.h"

namespace Lunex {

	RenderSystem::State* RenderSystem::s_State = nullptr;

	// ============================================================================
	// INITIALIZATION
	// ============================================================================
	
	void RenderSystem::Init(const RenderSystemConfig& config) {
		if (s_State) {
			LNX_LOG_WARN("RenderSystem already initialized!");
			return;
		}
		
		LNX_LOG_INFO("Initializing RenderSystem...");
		
		s_State = new State();
		s_State->Config = config;
		
		// Create render graph
		s_State->Graph = CreateScope<RenderGraph>();
		s_State->Graph->SetSwapchainSize(config.ViewportWidth, config.ViewportHeight);
		s_State->Graph->SetEnablePassCulling(true);
		
		// Create render passes (from GeometryPass.h)
		s_State->GeometryPassPtr = CreateScope<GeometryPass>();
		s_State->TransparentPassPtr = CreateScope<TransparentPass>();
		
		// Create render passes (from EnvironmentPass.h)
		s_State->SkyboxPassPtr = CreateScope<SkyboxPass>();
		s_State->IBLPassPtr = CreateScope<IBLPass>();
		
		// Create render passes (from EditorPass.h)
		s_State->GridPassPtr = CreateScope<GridPass>();
		s_State->GizmoPassPtr = CreateScope<GizmoPass>();
		s_State->SelectionOutlinePassPtr = CreateScope<SelectionOutlinePass>();
		s_State->DebugVisualizationPassPtr = CreateScope<DebugVisualizationPass>();
		
		s_State->Initialized = true;
		
		LNX_LOG_INFO("RenderSystem initialized successfully with 8 passes");
	}
	
	void RenderSystem::Shutdown() {
		if (!s_State) return;
		
		LNX_LOG_INFO("Shutting down RenderSystem...");
		
		delete s_State;
		s_State = nullptr;
	}

	// ============================================================================
	// FRAME LIFECYCLE
	// ============================================================================
	
	void RenderSystem::BeginFrame() {
		if (!s_State || !s_State->Initialized) return;
		
		// Reset render graph for new frame
		s_State->Graph->Reset();
	}
	
	void RenderSystem::EndFrame() {
		if (!s_State || !s_State->Initialized) return;
		// Nothing to do here for now
	}

	// ============================================================================
	// SCENE RENDERING
	// ============================================================================
	
	void RenderSystem::RenderScene(Scene* scene, const EditorCamera& camera) {
		if (!s_State || !s_State->Initialized || !scene) return;
		
		// Setup scene info
		s_State->CurrentSceneInfo.ScenePtr = scene;
		s_State->CurrentSceneInfo.View = ViewInfo::FromEditorCamera(
			camera, 
			s_State->Config.ViewportWidth, 
			s_State->Config.ViewportHeight
		);
		s_State->CurrentSceneInfo.DrawGrid = s_State->DrawGrid;
		s_State->CurrentSceneInfo.DrawGizmos = s_State->DrawGizmos;
		
		// Build and execute render graph
		BuildRenderGraph();
		
		s_State->Graph->Compile();
		
		if (s_State->Config.ExportRenderGraph) {
			std::string graphViz = s_State->Graph->ExportGraphViz();
			LNX_LOG_INFO("RenderGraph:\n{0}", graphViz);
		}
		
		s_State->Graph->Execute();
	}
	
	void RenderSystem::RenderScene(Scene* scene, Entity cameraEntity) {
		if (!s_State || !s_State->Initialized || !scene) return;
		
		// Get camera component
		if (!cameraEntity.HasComponent<CameraComponent>()) {
			LNX_LOG_ERROR("RenderScene: Entity does not have CameraComponent");
			return;
		}
		
		auto& cameraComp = cameraEntity.GetComponent<CameraComponent>();
		auto& transform = cameraEntity.GetComponent<TransformComponent>();
		
		// Setup scene info
		s_State->CurrentSceneInfo.ScenePtr = scene;
		s_State->CurrentSceneInfo.View = ViewInfo::FromCamera(
			cameraComp.Camera,
			transform.GetTransform(),
			s_State->Config.ViewportWidth,
			s_State->Config.ViewportHeight
		);
		s_State->CurrentSceneInfo.DrawGrid = false;  // No grid in runtime
		s_State->CurrentSceneInfo.DrawGizmos = false;
		
		// Build and execute
		BuildRenderGraph();
		s_State->Graph->Compile();
		s_State->Graph->Execute();
	}

	// ============================================================================
	// RENDER GRAPH BUILDING
	// ============================================================================
	
	void RenderSystem::BuildRenderGraph() {
		auto& graph = *s_State->Graph;
		auto& sceneInfo = s_State->CurrentSceneInfo;
		
		// ============================================
		// GEOMETRY PASS (opaque meshes)
		// ============================================
		
		graph.AddPass(
			"Geometry",
			[&](RenderPassBuilder& builder) {
				s_State->GeometryPassPtr->Setup(graph, builder, sceneInfo);
			},
			[&](const RenderPassResources& resources) {
				s_State->GeometryPassPtr->Execute(resources, sceneInfo);
			}
		);
		
		// ============================================
		// SKYBOX PASS
		// ============================================
		
		if (s_State->SkyboxPassPtr->ShouldExecute(sceneInfo)) {
			s_State->SkyboxPassPtr->SetColorTarget(s_State->GeometryPassPtr->GetColorOutput());
			s_State->SkyboxPassPtr->SetDepthTarget(s_State->GeometryPassPtr->GetDepthOutput());
			
			graph.AddPass(
				"Skybox",
				[&](RenderPassBuilder& builder) {
					s_State->SkyboxPassPtr->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->SkyboxPassPtr->Execute(resources, sceneInfo);
				}
			);
		}
		
		// ============================================
		// TRANSPARENT PASS
		// ============================================
		
		s_State->TransparentPassPtr->SetColorTarget(s_State->GeometryPassPtr->GetColorOutput());
		s_State->TransparentPassPtr->SetDepthTarget(s_State->GeometryPassPtr->GetDepthOutput());
		
		graph.AddPass(
			"Transparent",
			[&](RenderPassBuilder& builder) {
				s_State->TransparentPassPtr->Setup(graph, builder, sceneInfo);
			},
			[&](const RenderPassResources& resources) {
				s_State->TransparentPassPtr->Execute(resources, sceneInfo);
			}
		);
		
		// ============================================
		// EDITOR PASSES
		// ============================================
		
		if (s_State->GridPassPtr->ShouldExecute(sceneInfo)) {
			s_State->GridPassPtr->SetColorTarget(s_State->GeometryPassPtr->GetColorOutput());
			s_State->GridPassPtr->SetDepthTarget(s_State->GeometryPassPtr->GetDepthOutput());
			
			graph.AddPass(
				"Grid",
				[&](RenderPassBuilder& builder) {
					s_State->GridPassPtr->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->GridPassPtr->Execute(resources, sceneInfo);
				}
			);
		}
		
		if (s_State->GizmoPassPtr->ShouldExecute(sceneInfo)) {
			s_State->GizmoPassPtr->SetColorTarget(s_State->GeometryPassPtr->GetColorOutput());
			s_State->GizmoPassPtr->SetSelectedEntity(s_State->SelectedEntityID);
			
			graph.AddPass(
				"Gizmos",
				[&](RenderPassBuilder& builder) {
					s_State->GizmoPassPtr->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->GizmoPassPtr->Execute(resources, sceneInfo);
				}
			);
		}
		
		if (s_State->SelectionOutlinePassPtr->ShouldExecute(sceneInfo)) {
			s_State->SelectionOutlinePassPtr->SetColorTarget(s_State->GeometryPassPtr->GetColorOutput());
			s_State->SelectionOutlinePassPtr->SetDepthTarget(s_State->GeometryPassPtr->GetDepthOutput());
			s_State->SelectionOutlinePassPtr->SetSelectedEntity(s_State->SelectedEntityID);
			
			graph.AddPass(
				"SelectionOutline",
				[&](RenderPassBuilder& builder) {
					s_State->SelectionOutlinePassPtr->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->SelectionOutlinePassPtr->Execute(resources, sceneInfo);
				}
			);
		}
		
		// ============================================
		// FINAL OUTPUT
		// ============================================
		
		s_State->FinalColorTarget = s_State->GeometryPassPtr->GetColorOutput();
		graph.SetBackbufferSource(s_State->FinalColorTarget);
	}

	// ============================================================================
	// PUBLIC API
	// ============================================================================
	
	Ref<RHI::RHITexture2D> RenderSystem::GetFinalOutput() {
		if (!s_State) return nullptr;
		return s_State->FinalTexture;
	}
	
	void RenderSystem::SetViewportSize(uint32_t width, uint32_t height) {
		if (!s_State) return;
		
		s_State->Config.ViewportWidth = width;
		s_State->Config.ViewportHeight = height;
		
		if (s_State->Graph) {
			s_State->Graph->SetSwapchainSize(width, height);
		}
	}
	
	RenderSystemConfig& RenderSystem::GetConfig() {
		static RenderSystemConfig dummy;
		return s_State ? s_State->Config : dummy;
	}
	
	const RenderGraph::Statistics& RenderSystem::GetStatistics() {
		static RenderGraph::Statistics dummy;
		return s_State && s_State->Graph ? s_State->Graph->GetStatistics() : dummy;
	}
	
	std::string RenderSystem::ExportRenderGraphViz() {
		if (!s_State || !s_State->Graph) return "";
		return s_State->Graph->ExportGraphViz();
	}
	
	void RenderSystem::SetDrawGrid(bool draw) {
		if (s_State) s_State->DrawGrid = draw;
	}
	
	void RenderSystem::SetDrawGizmos(bool draw) {
		if (s_State) s_State->DrawGizmos = draw;
	}
	
	void RenderSystem::SetSelectedEntity(int entityID) {
		if (s_State) s_State->SelectedEntityID = entityID;
	}
	
	int RenderSystem::GetEntityAtScreenPos(int x, int y) {
		// TODO: Implement entity picking from framebuffer
		return -1;
	}

} // namespace Lunex
