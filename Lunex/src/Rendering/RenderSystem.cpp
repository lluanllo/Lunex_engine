#include "stpch.h"
#include "RenderSystem.h"
#include "Log/Log.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

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
		
		// Create render passes
		s_State->GeometryPass = CreateScope<GeometryPass>();
		s_State->TransparentPass = CreateScope<TransparentPass>();
		s_State->SkyboxPass = CreateScope<SkyboxPass>();
		s_State->GridPass = CreateScope<GridPass>();
		s_State->GizmoPass = CreateScope<GizmoPass>();
		s_State->SelectionOutlinePass = CreateScope<SelectionOutlinePass>();
		
		s_State->Initialized = true;
		
		LNX_LOG_INFO("RenderSystem initialized successfully");
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
		
		// Graph is already executed in RenderScene
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
		
		// Build render graph
		BuildRenderGraph();
		
		// Compile and execute
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
		// GEOMETRY PASS
		// ============================================
		
		graph.AddPass(
			"Geometry",
			[&](RenderPassBuilder& builder) {
				s_State->GeometryPass->Setup(graph, builder, sceneInfo);
			},
			[&](const RenderPassResources& resources) {
				s_State->GeometryPass->Execute(resources, sceneInfo);
			}
		);
		
		// ============================================
		// SKYBOX PASS
		// ============================================
		
		if (s_State->SkyboxPass->ShouldExecute(sceneInfo)) {
			// Share render targets with geometry
			s_State->SkyboxPass->SetColorTarget(s_State->GeometryPass->GetColorOutput());
			s_State->SkyboxPass->SetDepthTarget(s_State->GeometryPass->GetDepthOutput());
			
			graph.AddPass(
				"Skybox",
				[&](RenderPassBuilder& builder) {
					s_State->SkyboxPass->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->SkyboxPass->Execute(resources, sceneInfo);
				}
			);
		}
		
		// ============================================
		// TRANSPARENT PASS
		// ============================================
		
		s_State->TransparentPass->SetColorTarget(s_State->GeometryPass->GetColorOutput());
		s_State->TransparentPass->SetDepthTarget(s_State->GeometryPass->GetDepthOutput());
		
		graph.AddPass(
			"Transparent",
			[&](RenderPassBuilder& builder) {
				s_State->TransparentPass->Setup(graph, builder, sceneInfo);
			},
			[&](const RenderPassResources& resources) {
				s_State->TransparentPass->Execute(resources, sceneInfo);
			}
		);
		
		// ============================================
		// EDITOR PASSES
		// ============================================
		
		if (s_State->GridPass->ShouldExecute(sceneInfo)) {
			s_State->GridPass->SetColorTarget(s_State->GeometryPass->GetColorOutput());
			s_State->GridPass->SetDepthTarget(s_State->GeometryPass->GetDepthOutput());
			
			graph.AddPass(
				"Grid",
				[&](RenderPassBuilder& builder) {
					s_State->GridPass->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->GridPass->Execute(resources, sceneInfo);
				}
			);
		}
		
		if (s_State->GizmoPass->ShouldExecute(sceneInfo)) {
			s_State->GizmoPass->SetColorTarget(s_State->GeometryPass->GetColorOutput());
			s_State->GizmoPass->SetSelectedEntity(s_State->SelectedEntityID);
			
			graph.AddPass(
				"Gizmos",
				[&](RenderPassBuilder& builder) {
					s_State->GizmoPass->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->GizmoPass->Execute(resources, sceneInfo);
				}
			);
		}
		
		if (s_State->SelectionOutlinePass->ShouldExecute(sceneInfo)) {
			s_State->SelectionOutlinePass->SetColorTarget(s_State->GeometryPass->GetColorOutput());
			s_State->SelectionOutlinePass->SetDepthTarget(s_State->GeometryPass->GetDepthOutput());
			s_State->SelectionOutlinePass->SetSelectedEntity(s_State->SelectedEntityID);
			
			graph.AddPass(
				"SelectionOutline",
				[&](RenderPassBuilder& builder) {
					s_State->SelectionOutlinePass->Setup(graph, builder, sceneInfo);
				},
				[&](const RenderPassResources& resources) {
					s_State->SelectionOutlinePass->Execute(resources, sceneInfo);
				}
			);
		}
		
		// ============================================
		// FINAL OUTPUT
		// ============================================
		
		// The geometry pass color output is our final output
		s_State->FinalColorTarget = s_State->GeometryPass->GetColorOutput();
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
		// TODO: Implement entity picking
		// This would:
		// 1. Read from entity ID attachment in framebuffer
		// 2. Return entity ID at pixel position
		return -1;
	}

} // namespace Lunex
