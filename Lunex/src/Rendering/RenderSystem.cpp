#include "stpch.h"
#include "RenderSystem.h"
#include "RenderPassJob.h"
#include "RenderPassDescriptor.h"
#include "SceneDataCollector.h"
#include "Backends/RasterBackend.h"
#include "Backends/RayTracingBackend.h"
#include "Log/Log.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/GridRenderer.h"
#include "Core/JobSystem/JobSystem.h"

// AAA Systems Integration
#include "Scene/Camera/CameraSystem.h"
#include "Scene/Lighting/LightSystem.h"

#include <unordered_set>

namespace Lunex {

	RenderSystem::State* RenderSystem::s_State = nullptr;

	// ============================================================================
	// BACKEND FACTORY
	// ============================================================================

	Scope<IRenderBackend> RenderSystem::CreateBackend(RenderMode mode) {
		switch (mode) {
			case RenderMode::Rasterization:
				return CreateScope<RasterBackend>();

			case RenderMode::RayTracing:
				return CreateScope<RayTracingBackend>();

			case RenderMode::Hybrid:
				LNX_LOG_WARN("Hybrid backend not yet implemented — falling back to Rasterization");
				return CreateScope<RasterBackend>();

			default:
				LNX_LOG_ERROR("Unknown RenderMode {0} — falling back to Rasterization", static_cast<int>(mode));
				return CreateScope<RasterBackend>();
		}
	}

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
		
		// ============================================
		// CREATE RENDER BACKEND (Phase 1)
		// ============================================
		s_State->CurrentRenderMode = config.InitialRenderMode;
		s_State->ActiveBackend = CreateBackend(config.InitialRenderMode);
		if (s_State->ActiveBackend) {
			s_State->ActiveBackend->Initialize(config);
		}
		
		// Initialize AAA systems if enabled
		if (config.UseCameraSystem) {
			CameraSystem::Get().Initialize();
			LNX_LOG_INFO("CameraSystem initialized");
		}
		
		if (config.UseLightSystem) {
			LightSystem::Get().Initialize();
			LNX_LOG_INFO("LightSystem initialized");
		}
		
		s_State->Initialized = true;
		
		LNX_LOG_INFO("RenderSystem initialized (backend={0}, parallel={1}, CameraSystem={2}, LightSystem={3})", 
			s_State->ActiveBackend ? s_State->ActiveBackend->GetName() : "none",
			config.EnableParallelPasses ? "enabled" : "disabled",
			config.UseCameraSystem ? "enabled" : "disabled",
			config.UseLightSystem ? "enabled" : "disabled");
	}
	
	void RenderSystem::Shutdown() {
		if (!s_State) return;
		
		LNX_LOG_INFO("Shutting down RenderSystem...");
		
		// Wait for any pending jobs
		if (s_State->JobScheduler) {
			s_State->JobScheduler->WaitForCompletion();
		}
		
		// Shutdown active backend
		if (s_State->ActiveBackend) {
			s_State->ActiveBackend->Shutdown();
			s_State->ActiveBackend.reset();
		}
		
		// Shutdown AAA systems
		if (s_State->Config.UseCameraSystem) {
			CameraSystem::Get().Shutdown();
		}
		
		if (s_State->Config.UseLightSystem) {
			LightSystem::Get().Shutdown();
		}
		
		delete s_State;
		s_State = nullptr;
	}

	// ============================================================================
	// RENDER BACKEND API (Phase 1)
	// ============================================================================

	void RenderSystem::SetRenderMode(RenderMode mode) {
		if (!s_State || !s_State->Initialized) {
			LNX_LOG_ERROR("RenderSystem not initialized — cannot set render mode");
			return;
		}

		if (s_State->CurrentRenderMode == mode && s_State->ActiveBackend) {
			LNX_LOG_INFO("Already using {0} backend", RenderModeToString(mode));
			return;
		}

		LNX_LOG_INFO("Switching render backend: {0} -> {1}",
			RenderModeToString(s_State->CurrentRenderMode),
			RenderModeToString(mode));

		// Wait for any in-flight work
		if (s_State->JobScheduler)
			s_State->JobScheduler->WaitForCompletion();

		// Shutdown old backend
		if (s_State->ActiveBackend) {
			s_State->ActiveBackend->Shutdown();
			s_State->ActiveBackend.reset();
		}

		// Create and initialize new backend
		s_State->ActiveBackend = CreateBackend(mode);
		if (s_State->ActiveBackend)
			s_State->ActiveBackend->Initialize(s_State->Config);

		s_State->CurrentRenderMode = mode;

		// Force graph rebuild on next frame
		if (s_State->Graph)
			s_State->Graph->Reset();

		LNX_LOG_INFO("Render backend switched to: {0}",
			s_State->ActiveBackend ? s_State->ActiveBackend->GetName() : "none");
	}

	RenderMode RenderSystem::GetRenderMode() {
		return s_State ? s_State->CurrentRenderMode : RenderMode::Rasterization;
	}

	IRenderBackend* RenderSystem::GetActiveBackend() {
		return s_State ? s_State->ActiveBackend.get() : nullptr;
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
		
		// Invalidate cached data
		s_State->CameraDataValid = false;
		s_State->LightingDataValid = false;
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
		
		// Sync AAA systems with scene
		SyncSystemsWithScene(scene);
		
		// Setup scene info
		s_State->CurrentSceneInfo.ScenePtr = scene;
		s_State->CurrentSceneInfo.View = ViewInfo::FromEditorCamera(
			camera, 
			s_State->Config.ViewportWidth, 
			s_State->Config.ViewportHeight
		);
		s_State->CurrentSceneInfo.DrawGrid = s_State->DrawGrid;
		s_State->CurrentSceneInfo.DrawGizmos = s_State->DrawGizmos;
		
		// Cull lights if enabled
		if (s_State->Config.UseLightSystem && s_State->Config.EnableLightCulling) {
			ViewFrustum frustum(s_State->CurrentSceneInfo.View.ViewProjectionMatrix);
			LightSystem::Get().CullLights(frustum);
		}
		
		// Collect scene data for backend
		s_State->CurrentSceneData = SceneDataCollector::Collect(
			scene, GetActiveCameraData(), GetLightingData(), true);
		s_State->CurrentSceneData.SelectedEntityID = s_State->SelectedEntityID;
		
		// Let backend prepare GPU data
		if (s_State->ActiveBackend)
			s_State->ActiveBackend->PrepareSceneData(s_State->CurrentSceneData);
		
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
		
		if (!cameraEntity.HasComponent<CameraComponent>()) {
			LNX_LOG_ERROR("RenderScene: Entity does not have CameraComponent");
			return;
		}
		
		// Sync AAA systems with scene
		SyncSystemsWithScene(scene);
		
		auto& cameraComp = cameraEntity.GetComponent<CameraComponent>();
		auto& transform = cameraEntity.GetComponent<TransformComponent>();
		
		s_State->CurrentSceneInfo.ScenePtr = scene;
		s_State->CurrentSceneInfo.View = ViewInfo::FromCamera(
			cameraComp.Camera,
			transform.GetTransform(),
			s_State->Config.ViewportWidth,
			s_State->Config.ViewportHeight
		);
		s_State->CurrentSceneInfo.DrawGrid = false;
		s_State->CurrentSceneInfo.DrawGizmos = false;
		
		// Cull lights if enabled
		if (s_State->Config.UseLightSystem && s_State->Config.EnableLightCulling) {
			ViewFrustum frustum(s_State->CurrentSceneInfo.View.ViewProjectionMatrix);
			LightSystem::Get().CullLights(frustum);
		}
		
		// Collect scene data for backend
		s_State->CurrentSceneData = SceneDataCollector::Collect(
			scene, GetActiveCameraData(), GetLightingData(), false);
		
		// Let backend prepare GPU data
		if (s_State->ActiveBackend)
			s_State->ActiveBackend->PrepareSceneData(s_State->CurrentSceneData);
		
		BuildRenderGraph();
		s_State->Graph->Compile();
		
		if (s_State->Config.EnableParallelPasses) {
			ExecuteRenderGraphParallel();
		} else {
			s_State->Graph->Execute();
		}
	}

	// ============================================================================
	// AAA SYSTEMS INTEGRATION
	// ============================================================================
	
	void RenderSystem::SyncSystemsWithScene(Scene* scene) {
		if (!s_State || !scene) return;
		
		SyncCameraSystem(scene);
		SyncLightSystem(scene);
	}
	
	void RenderSystem::SyncCameraSystem(Scene* scene) {
		if (!s_State->Config.UseCameraSystem) return;
		
		CameraSystem::Get().SyncFromScene(scene);
		s_State->CameraDataValid = false;
	}
	
	void RenderSystem::SyncLightSystem(Scene* scene) {
		if (!s_State->Config.UseLightSystem) return;
		
		LightSystem::Get().SyncFromScene(scene);
		s_State->LightingDataValid = false;
	}
	
	CameraRenderData RenderSystem::GetActiveCameraData() {
		if (!s_State) return CameraRenderData{};
		
		// Use cached data if valid
		if (s_State->CameraDataValid) {
			return s_State->CachedCameraData;
		}
		
		// Get from CameraSystem if enabled
		if (s_State->Config.UseCameraSystem) {
			// TODO: CameraSystem::Get().GetActiveCameraData() - needs implementation
			// For now, fallback to ViewInfo
		}
		
		// Fallback: construct from current view info
		auto& view = s_State->CurrentSceneInfo.View;
		CameraRenderData data;
		data.ViewMatrix = view.ViewMatrix;
		data.ProjectionMatrix = view.ProjectionMatrix;
		data.ViewProjectionMatrix = view.ViewProjectionMatrix;
		data.InverseViewMatrix = glm::inverse(view.ViewMatrix);
		data.InverseProjectionMatrix = glm::inverse(view.ProjectionMatrix);
		data.Position = view.CameraPosition;
		data.Direction = view.CameraDirection;
		data.NearPlane = view.NearPlane;
		data.FarPlane = view.FarPlane;
		data.FieldOfView = 45.0f;
		data.AspectRatio = view.AspectRatio;
		data.IsPerspective = true;
		
		s_State->CachedCameraData = data;
		s_State->CameraDataValid = true;
		
		return data;
	}
	
	LightingData RenderSystem::GetLightingData() {
		if (!s_State) return LightingData{};
		
		// Use cached data if valid
		if (s_State->LightingDataValid) {
			return s_State->CachedLightingData;
		}
		
		// Get from LightSystem if enabled
		if (s_State->Config.UseLightSystem) {
			s_State->CachedLightingData = LightSystem::Get().GetLightingData();
			s_State->LightingDataValid = true;
			return s_State->CachedLightingData;
		}
		
		// Fallback: empty lighting
		s_State->CachedLightingData = LightingData{};
		s_State->LightingDataValid = true;
		return s_State->CachedLightingData;
	}

	// ============================================================================
	// RENDER GRAPH BUILDING
	// ============================================================================
	
	void RenderSystem::BuildRenderGraph() {
		auto& graph = *s_State->Graph;
		auto& sceneInfo = s_State->CurrentSceneInfo;
		
		// Delegate to the active backend
		if (s_State->ActiveBackend) {
			// Forward editor state to backend
			if (auto* raster = dynamic_cast<RasterBackend*>(s_State->ActiveBackend.get())) {
				raster->SetDrawGrid(s_State->DrawGrid);
				raster->SetDrawGizmos(s_State->DrawGizmos);
				raster->SetSelectedEntity(s_State->SelectedEntityID);
			}
			
			s_State->ActiveBackend->BuildRenderGraph(graph, sceneInfo);
			s_State->FinalColorTarget = s_State->ActiveBackend->GetFinalColorOutput();
			return;
		}
		
		// If no backend is active, this is an error state — should not happen
		LNX_LOG_ERROR("RenderSystem::BuildRenderGraph called with no active backend!");
	}
	
	// ============================================================================
	// PARALLEL RENDER GRAPH EXECUTION
	// ============================================================================
	
	void RenderSystem::ExecuteRenderGraphParallel() {
		auto& sceneInfo = s_State->CurrentSceneInfo;
		auto& scheduler = *s_State->JobScheduler;
		
		// Get passes from backend for job scheduling
		auto* raster = dynamic_cast<RasterBackend*>(s_State->ActiveBackend.get());
		if (!raster) {
			// Non-raster backends: just execute the graph sequentially for now
			s_State->Graph->Execute();
			return;
		}
		
		// Create jobs for each render pass (same logic as before, now via backend)
		auto geometryJob = CreateRef<RenderPassJob>(raster->GetGeometryPass(), s_State->Graph.get());
		scheduler.AddJob(geometryJob);
		
		if (raster->GetSkyboxPass()->ShouldExecute(sceneInfo)) {
			auto skyboxJob = CreateRef<RenderPassJob>(raster->GetSkyboxPass(), s_State->Graph.get());
			skyboxJob->AddDependency(geometryJob.get());
			scheduler.AddJob(skyboxJob);
		}
		
		auto transparentJob = CreateRef<RenderPassJob>(raster->GetTransparentPass(), s_State->Graph.get());
		transparentJob->AddDependency(geometryJob.get());
		scheduler.AddJob(transparentJob);
		
		if (raster->GetGridPass()->ShouldExecute(sceneInfo)) {
			auto gridJob = CreateRef<RenderPassJob>(raster->GetGridPass(), s_State->Graph.get());
			gridJob->AddDependency(transparentJob.get());
			scheduler.AddJob(gridJob);
		}
		
		if (raster->GetGizmoPass()->ShouldExecute(sceneInfo)) {
			auto gizmoJob = CreateRef<RenderPassJob>(raster->GetGizmoPass(), s_State->Graph.get());
			gizmoJob->AddDependency(transparentJob.get());
			scheduler.AddJob(gizmoJob);
		}
		
		if (raster->GetSelectionOutlinePass()->ShouldExecute(sceneInfo)) {
			auto outlineJob = CreateRef<RenderPassJob>(raster->GetSelectionOutlinePass(), s_State->Graph.get());
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
		
		if (s_State->ActiveBackend) {
			s_State->ActiveBackend->OnViewportResize(width, height);
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

	// ============================================================================
	// RAY TRACING API (Phase 2)
	// ============================================================================

	void RenderSystem::ResetRTAccumulation() {
		if (!s_State || !s_State->ActiveBackend) return;
		if (auto* rt = dynamic_cast<RayTracingBackend*>(s_State->ActiveBackend.get())) {
			rt->ResetAccumulation();
		}
	}

	uint32_t RenderSystem::GetRTAccumulatedFrames() {
		if (!s_State || !s_State->ActiveBackend) return 0;
		if (auto* rt = dynamic_cast<RayTracingBackend*>(s_State->ActiveBackend.get())) {
			return rt->GetAccumulatedFrames();
		}
		return 0;
	}

} // namespace Lunex
