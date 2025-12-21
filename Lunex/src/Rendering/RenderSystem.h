#pragma once

/**
 * @file RenderSystem.h
 * @brief High-level rendering system facade
 * 
 * AAA-level architecture with:
 * - Data-driven pass registration via RenderPassDescriptor
 * - Automatic dependency resolution
 * - Parallel execution support
 * - Resource lifetime management
 */

#include "Core/Core.h"
#include "RenderGraph.h"
#include "RenderPass.h"
#include "RenderPassDescriptor.h"
#include "RenderPassJob.h"
#include "Passes/GeometryPass.h"
#include "Passes/EnvironmentPass.h"
#include "Passes/EditorPass.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Lunex {

	// Forward declarations
	class EnvironmentMap;

	// ============================================================================
	// RENDER SYSTEM CONFIG
	// ============================================================================
	
	/**
	 * @struct RenderSystemConfig
	 * @brief Configuration for the render system
	 */
	struct RenderSystemConfig {
		uint32_t ViewportWidth = 1920;
		uint32_t ViewportHeight = 1080;
		
		bool EnableMSAA = false;
		uint32_t MSAASamples = 4;
		
		bool EnableVSync = true;
		
		// Quality settings
		bool EnableShadows = true;
		uint32_t ShadowMapSize = 2048;
		
		bool EnableSSAO = false;
		bool EnableBloom = false;
		bool EnableHDR = true;
		float Exposure = 1.0f;
		
		// Debug
		bool EnableDebugOutput = true;
		bool ExportRenderGraph = false;
		
		// ============================================
		// JOB SYSTEM INTEGRATION
		// ============================================
		
		/**
		 * @brief Enable parallel pass execution via JobSystem
		 * When enabled, independent passes will be scheduled as jobs
		 */
		bool EnableParallelPasses = true;
		
		/**
		 * @brief Enable parallel draw command collection
		 * When enabled, entity iteration for draw commands uses ParallelFor
		 */
		bool EnableParallelDrawCollection = true;
		
		/**
		 * @brief Minimum entity count to trigger parallel collection
		 */
		uint32_t ParallelCollectionThreshold = 100;
	};

	// ============================================================================
	// RENDER SYSTEM
	// ============================================================================
	
	/**
	 * @class RenderSystem
	 * @brief Main rendering system facade
	 * 
	 * Usage (data-driven):
	 * ```cpp
	 * // Register passes via descriptors
	 * RenderSystem::RegisterPass(RenderPassDescriptor::Graphics(
	 *     "MyPass",
	 *     PassCategory::ForwardOpaque,
	 *     { inputs... },
	 *     { outputs... },
	 *     [](auto& cmd, auto& res, auto& scene) { ... }
	 * ));
	 * 
	 * // Or use automatic registration with REGISTER_RENDER_PASS macro
	 * ```
	 */
	class RenderSystem {
	public:
		/**
		 * @brief Initialize the render system
		 */
		static void Init(const RenderSystemConfig& config = RenderSystemConfig{});
		
		/**
		 * @brief Shutdown the render system
		 */
		static void Shutdown();
		
		/**
		 * @brief Begin a new frame
		 */
		static void BeginFrame();
		
		/**
		 * @brief End the current frame
		 */
		static void EndFrame();
		
		/**
		 * @brief Render a scene with editor camera
		 */
		static void RenderScene(Scene* scene, const EditorCamera& camera);
		
		/**
		 * @brief Render a scene with runtime camera entity
		 */
		static void RenderScene(Scene* scene, Entity cameraEntity);
		
		/**
		 * @brief Get the final render output texture
		 */
		static Ref<RHI::RHITexture2D> GetFinalOutput();
		
		/**
		 * @brief Resize viewport
		 */
		static void SetViewportSize(uint32_t width, uint32_t height);
		
		/**
		 * @brief Get current configuration
		 */
		static RenderSystemConfig& GetConfig();
		
		/**
		 * @brief Get render statistics
		 */
		static const RenderGraph::Statistics& GetStatistics();
		
		/**
		 * @brief Get job scheduler statistics
		 */
		static const RenderJobScheduler::Statistics& GetJobSchedulerStatistics();
		
		/**
		 * @brief Export render graph visualization
		 */
		static std::string ExportRenderGraphViz();
		
		// ============================================
		// DATA-DRIVEN PASS REGISTRATION (NEW AAA API)
		// ============================================
		
		/**
		 * @brief Register a pass from descriptor
		 */
		static void RegisterPass(const RenderPassDescriptor& descriptor);
		
		/**
		 * @brief Unregister a pass by name
		 */
		static void UnregisterPass(const std::string& name);
		
		/**
		 * @brief Enable/disable a registered pass at runtime
		 */
		static void SetPassEnabled(const std::string& name, bool enabled);
		
		/**
		 * @brief Check if a pass is enabled
		 */
		static bool IsPassEnabled(const std::string& name);
		
		/**
		 * @brief Get all registered pass names
		 */
		static std::vector<std::string> GetRegisteredPasses();
		
		// ============================================
		// EDITOR API
		// ============================================
		
		/**
		 * @brief Enable/disable grid rendering
		 */
		static void SetDrawGrid(bool draw);
		
		/**
		 * @brief Enable/disable gizmo rendering
		 */
		static void SetDrawGizmos(bool draw);
		
		/**
		 * @brief Set selected entity for editor highlighting
		 */
		static void SetSelectedEntity(int entityID);
		
		/**
		 * @brief Get entity ID at screen position (picking)
		 */
		static int GetEntityAtScreenPos(int x, int y);
		
		// ============================================
		// PARALLEL EXECUTION API
		// ============================================
		
		/**
		 * @brief Enable/disable parallel pass execution
		 */
		static void SetParallelPassesEnabled(bool enabled);
		
		/**
		 * @brief Check if parallel passes are enabled
		 */
		static bool IsParallelPassesEnabled();
		
	private:
		RenderSystem() = delete;
		~RenderSystem() = delete;
		
		static void BuildRenderGraph();
		static void BuildRenderGraphFromDescriptors();
		static void ExecuteRenderGraph();
		static void ExecuteRenderGraphParallel();
		
		// Internal state
		struct State {
			RenderSystemConfig Config;
			
			// RenderGraph
			Scope<RenderGraph> Graph;
			
			// Job Scheduler for parallel pass execution
			Scope<RenderJobScheduler> JobScheduler;
			
			// Legacy render passes (kept for compatibility)
			Scope<GeometryPass> GeometryPassPtr;
			Scope<TransparentPass> TransparentPassPtr;
			Scope<SkyboxPass> SkyboxPassPtr;
			Scope<IBLPass> IBLPassPtr;
			Scope<GridPass> GridPassPtr;
			Scope<GizmoPass> GizmoPassPtr;
			Scope<SelectionOutlinePass> SelectionOutlinePassPtr;
			Scope<DebugVisualizationPass> DebugVisualizationPassPtr;
			
			// Data-driven pass descriptors (NEW)
			std::unordered_map<std::string, RenderPassDescriptor> RegisteredPasses;
			bool UseDataDrivenPasses = false;  // Toggle between legacy and new system
			
			// Scene info for current frame
			SceneRenderInfo CurrentSceneInfo;
			
			// Output
			RenderGraphResource FinalColorTarget;
			Ref<RHI::RHITexture2D> FinalTexture;
			
			// Editor state
			bool DrawGrid = true;
			bool DrawGizmos = true;
			int SelectedEntityID = -1;
			
			// Scene version for job cancellation
			uint64_t CurrentSceneVersion = 0;
			
			bool Initialized = false;
		};
		
		static State* s_State;
	};

} // namespace Lunex
