#include "stpch.h"
#include "RenderSystem.h"
#include "RenderPassJob.h"
#include "RenderPassDescriptor.h"
#include "Log/Log.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/GridRenderer.h"
#include "Core/JobSystem/JobSystem.h"
#include <unordered_set>

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
		
		// Create job scheduler for parallel pass execution
		s_State->JobScheduler = CreateScope<RenderJobScheduler>();
		
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
		
		LNX_LOG_INFO("RenderSystem initialized successfully with 8 passes (parallel={0})", 
			config.EnableParallelPasses ? "enabled" : "disabled");
	}
	
	void RenderSystem::Shutdown() {
		if (!s_State) return;
		
		LNX_LOG_INFO("Shutting down RenderSystem...");
		
		// Wait for any pending jobs
		if (s_State->JobScheduler) {
			s_State->JobScheduler->WaitForCompletion();
		}
		
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
		
		// Clear job scheduler
		s_State->JobScheduler->Clear();
		
		// Increment scene version for job cancellation
		s_State->CurrentSceneVersion++;
	}
	
	void RenderSystem::EndFrame() {
		if (!s_State || !s_State->Initialized) return;
		
		// Wait for any pending parallel jobs
		if (s_State->Config.EnableParallelPasses) {
			s_State->JobScheduler->WaitForCompletion();
		}
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
		
		// Execute with parallel passes if enabled
		if (s_State->Config.EnableParallelPasses) {
			ExecuteRenderGraphParallel();
		} else {
			s_State->Graph->Execute();
		}
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
		
		if (s_State->Config.EnableParallelPasses) {
			ExecuteRenderGraphParallel();
		} else {
			s_State->Graph->Execute();
		}
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
	// PARALLEL RENDER GRAPH EXECUTION
	// ============================================================================
	
	void RenderSystem::ExecuteRenderGraphParallel() {
		auto& sceneInfo = s_State->CurrentSceneInfo;
		auto& scheduler = *s_State->JobScheduler;
		
		// Create jobs for each render pass
		// Note: OpenGL requires most operations on main thread, so for now
		// we schedule sequentially but with JobSystem infrastructure ready
		
		// GeometryPass Job (depends on nothing)
		auto geometryJob = CreateRef<RenderPassJob>(s_State->GeometryPassPtr.get(), s_State->Graph.get());
		scheduler.AddJob(geometryJob);
		
		// SkyboxPass Job (depends on GeometryPass)
		if (s_State->SkyboxPassPtr->ShouldExecute(sceneInfo)) {
			auto skyboxJob = CreateRef<RenderPassJob>(s_State->SkyboxPassPtr.get(), s_State->Graph.get());
			skyboxJob->AddDependency(geometryJob.get());
			scheduler.AddJob(skyboxJob);
		}
		
		// TransparentPass Job (depends on GeometryPass)
		auto transparentJob = CreateRef<RenderPassJob>(s_State->TransparentPassPtr.get(), s_State->Graph.get());
		transparentJob->AddDependency(geometryJob.get());
		scheduler.AddJob(transparentJob);
		
		// Editor passes (depend on previous passes)
		if (s_State->GridPassPtr->ShouldExecute(sceneInfo)) {
			auto gridJob = CreateRef<RenderPassJob>(s_State->GridPassPtr.get(), s_State->Graph.get());
			gridJob->AddDependency(transparentJob.get());
			scheduler.AddJob(gridJob);
		}
		
		if (s_State->GizmoPassPtr->ShouldExecute(sceneInfo)) {
			auto gizmoJob = CreateRef<RenderPassJob>(s_State->GizmoPassPtr.get(), s_State->Graph.get());
			gizmoJob->AddDependency(transparentJob.get());
			scheduler.AddJob(gizmoJob);
		}
		
		if (s_State->SelectionOutlinePassPtr->ShouldExecute(sceneInfo)) {
			auto outlineJob = CreateRef<RenderPassJob>(s_State->SelectionOutlinePassPtr.get(), s_State->Graph.get());
			outlineJob->AddDependency(transparentJob.get());
			scheduler.AddJob(outlineJob);
		}
		
		// Prepare all jobs with scene info
		RenderPassJobData jobData;
		jobData.SceneInfo = &sceneInfo;
		jobData.SceneVersion = s_State->CurrentSceneVersion;
		
		// Execute all scheduled jobs
		auto counter = scheduler.Execute(s_State->CurrentSceneVersion);
		
		// For OpenGL, we must wait for completion (no async submit)
		scheduler.WaitForCompletion();
	}

	// ============================================================================
	// DATA-DRIVEN PASS REGISTRATION (NEW AAA API)
	// ============================================================================
	
	void RenderSystem::RegisterPass(const RenderPassDescriptor& descriptor) {
		if (!s_State) {
			LNX_LOG_ERROR("RenderSystem not initialized - cannot register pass: {0}", descriptor.Name);
			return;
		}
		
		if (descriptor.Name.empty()) {
			LNX_LOG_ERROR("Cannot register pass with empty name");
			return;
		}
		
		if (!descriptor.Execute) {
			LNX_LOG_ERROR("Cannot register pass '{0}' without execute function", descriptor.Name);
			return;
		}
		
		// Add to local registry
		s_State->RegisteredPasses[descriptor.Name] = descriptor;
		
		// Also add to global PassRegistry
		PassRegistry::Get().Register(descriptor);
		
		LNX_LOG_INFO("Registered render pass: {0} (category: {1}, priority: {2})", 
			descriptor.Name, 
			static_cast<int>(descriptor.Category),
			descriptor.Priority);
	}
	
	void RenderSystem::UnregisterPass(const std::string& name) {
		if (!s_State) return;
		
		s_State->RegisteredPasses.erase(name);
		PassRegistry::Get().Unregister(name);
		
		LNX_LOG_INFO("Unregistered render pass: {0}", name);
	}
	
	void RenderSystem::SetPassEnabled(const std::string& name, bool enabled) {
		if (!s_State) return;
		
		auto it = s_State->RegisteredPasses.find(name);
		if (it != s_State->RegisteredPasses.end()) {
			it->second.Enabled = enabled;
			LNX_LOG_INFO("Render pass '{0}' {1}", name, enabled ? "enabled" : "disabled");
		}
	}
	
	bool RenderSystem::IsPassEnabled(const std::string& name) {
		if (!s_State) return false;
		
		auto it = s_State->RegisteredPasses.find(name);
		if (it != s_State->RegisteredPasses.end()) {
			return it->second.Enabled;
		}
		return false;
	}
	
	std::vector<std::string> RenderSystem::GetRegisteredPasses() {
		std::vector<std::string> names;
		if (!s_State) return names;
		
		names.reserve(s_State->RegisteredPasses.size());
		for (const auto& [name, desc] : s_State->RegisteredPasses) {
			names.push_back(name);
		}
		return names;
	}
	
	// ============================================================================
	// DATA-DRIVEN RENDER GRAPH BUILDING
	// ============================================================================
	
	void RenderSystem::BuildRenderGraphFromDescriptors() {
		if (!s_State || s_State->RegisteredPasses.empty()) return;
		
		auto& graph = *s_State->Graph;
		auto& sceneInfo = s_State->CurrentSceneInfo;
		
		// Get sorted passes from registry
		auto sortedPasses = PassRegistry::Get().GetSortedPasses();
		
		// Resource handles map (name -> handle)
		std::unordered_map<std::string, RenderGraphResource> resourceHandles;
		
		// Track which resources have been created
		std::unordered_set<std::string> createdResources;
		
		for (const RenderPassDescriptor* passDesc : sortedPasses) {
			// Check condition
			if (passDesc->Condition && !passDesc->Condition(sceneInfo)) {
				continue;
			}
			
			// Capture descriptor pointer for lambda
			const RenderPassDescriptor* desc = passDesc;
			
			graph.AddPass(
				desc->Name,
				// Setup function
				[&, desc](RenderPassBuilder& builder) {
					builder.SetName(desc->Name);
					
					// Process outputs first (they might create resources)
					for (const auto& output : desc->Outputs) {
						RenderGraphResource handle;
						
						// Check if resource exists
						auto it = resourceHandles.find(output.Name);
						if (it != resourceHandles.end()) {
							handle = it->second;
						} else {
							// Create new resource
							if (output.IsTexture) {
								handle = builder.CreateTexture(output.TextureDesc);
							} else {
								handle = builder.CreateBuffer(output.BufferDesc);
							}
							resourceHandles[output.Name] = handle;
							createdResources.insert(output.Name);
						}
						
						// Set as render target if needed
						if (output.Access == ResourceAccess::RenderTarget) {
							builder.SetRenderTarget(handle, output.Slot);
						} else if (output.Access == ResourceAccess::DepthTarget) {
							builder.SetDepthTarget(handle);
						} else {
							builder.WriteTexture(handle);
						}
					}
					
					// Process inputs
					for (const auto& input : desc->Inputs) {
						auto it = resourceHandles.find(input.Name);
						if (it != resourceHandles.end()) {
							if (input.IsTexture) {
								builder.ReadTexture(it->second);
							} else {
								builder.ReadBuffer(it->second);
							}
						} else {
							LNX_LOG_WARN("Pass '{0}' reads undefined resource: {1}", 
								desc->Name, input.Name);
						}
					}
				},
				// Execute function
				[&, desc](const RenderPassResources& resources) {
					auto* cmdList = resources.GetCommandList();
					if (cmdList && desc->Execute) {
						desc->Execute(*cmdList, resources, sceneInfo);
					}
				}
			);
		}
		
		// Set final output (last color render target)
		if (!resourceHandles.empty()) {
			// Find "FinalColor" or similar
			auto it = resourceHandles.find("FinalColor");
			if (it != resourceHandles.end()) {
				s_State->FinalColorTarget = it->second;
				graph.SetBackbufferSource(s_State->FinalColorTarget);
			} else {
				// Use last created color target
				for (const auto& [name, handle] : resourceHandles) {
					s_State->FinalColorTarget = handle;
					graph.SetBackbufferSource(s_State->FinalColorTarget);
				}
			}
		}
	}

	void RenderSystem::ExecuteRenderGraph() {
		if (!s_State || !s_State->Graph) return;
		s_State->Graph->Execute();
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
	
	const RenderJobScheduler::Statistics& RenderSystem::GetJobSchedulerStatistics() {
		static RenderJobScheduler::Statistics dummy;
		return s_State && s_State->JobScheduler ? s_State->JobScheduler->GetStatistics() : dummy;
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
	
	void RenderSystem::SetParallelPassesEnabled(bool enabled) {
		if (s_State) s_State->Config.EnableParallelPasses = enabled;
	}
	
	bool RenderSystem::IsParallelPassesEnabled() {
		return s_State ? s_State->Config.EnableParallelPasses : false;
	}

} // namespace Lunex
