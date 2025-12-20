#pragma once

/**
 * @file RenderSystem.h
 * @brief High-level rendering system facade
 */

#include "Core/Core.h"
#include "RenderGraph.h"
#include "RenderPass.h"
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
	};

	// ============================================================================
	// RENDER SYSTEM
	// ============================================================================
	
	/**
	 * @class RenderSystem
	 * @brief Main rendering system facade
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
		 * @brief Export render graph visualization
		 */
		static std::string ExportRenderGraphViz();
		
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
		
	private:
		RenderSystem() = delete;
		~RenderSystem() = delete;
		
		static void BuildRenderGraph();
		static void ExecuteRenderGraph();
		
		// Internal state
		struct State {
			RenderSystemConfig Config;
			
			// RenderGraph
			Scope<RenderGraph> Graph;
			
			// Render passes (from GeometryPass.h)
			Scope<GeometryPass> GeometryPassPtr;
			Scope<TransparentPass> TransparentPassPtr;
			
			// Render passes (from EnvironmentPass.h)
			Scope<SkyboxPass> SkyboxPassPtr;
			Scope<IBLPass> IBLPassPtr;
			
			// Render passes (from EditorPass.h)
			Scope<GridPass> GridPassPtr;
			Scope<GizmoPass> GizmoPassPtr;
			Scope<SelectionOutlinePass> SelectionOutlinePassPtr;
			Scope<DebugVisualizationPass> DebugVisualizationPassPtr;
			
			// Scene info for current frame
			SceneRenderInfo CurrentSceneInfo;
			
			// Output
			RenderGraphResource FinalColorTarget;
			Ref<RHI::RHITexture2D> FinalTexture;
			
			// Editor state
			bool DrawGrid = true;
			bool DrawGizmos = true;
			int SelectedEntityID = -1;
			
			bool Initialized = false;
		};
		
		static State* s_State;
	};

} // namespace Lunex
