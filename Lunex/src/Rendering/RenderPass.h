#pragma once

/**
 * @file RenderPass.h
 * @brief Base class and common utilities for render passes
 */

#include "Core/Core.h"
#include "RenderGraph.h"
#include "DrawCommand.h"
#include "Scene/Scene.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/EditorCamera.h"
#include <glm/glm.hpp>

namespace Lunex {

	// Forward declarations
	class Scene;
	class EditorCamera;

	// ============================================================================
	// VIEW INFO
	// ============================================================================
	
	/**
	 * @struct ViewInfo
	 * @brief Camera and viewport information for a render pass
	 */
	struct ViewInfo {
		glm::mat4 ViewMatrix = glm::mat4(1.0f);
		glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
		glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
		glm::vec3 CameraPosition = glm::vec3(0.0f);
		glm::vec3 CameraDirection = glm::vec3(0.0f, 0.0f, -1.0f);  // NEW
		
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;
		float AspectRatio = 16.0f / 9.0f;  // NEW
		
		uint32_t ViewportWidth = 1920;
		uint32_t ViewportHeight = 1080;
		
		bool IsEditorCamera = false;  // NEW: Distinguish editor vs runtime camera
		
		static ViewInfo FromEditorCamera(const EditorCamera& camera, uint32_t width, uint32_t height);
		static ViewInfo FromCamera(const Camera& camera, const glm::mat4& transform, uint32_t width, uint32_t height);
	};

	// ============================================================================
	// SCENE RENDER INFO
	// ============================================================================
	
	/**
	 * @struct SceneRenderInfo
	 * @brief Complete information needed to render a scene
	 */
	struct SceneRenderInfo {
		Scene* ScenePtr = nullptr;
		ViewInfo View;
		
		// Lighting (will be populated by the render system)
		std::vector<glm::vec4> DirectionalLights;  // xyz = direction, w = intensity
		std::vector<glm::vec4> PointLights;        // xyz = position, w = radius
		
		// Environment
		Ref<class EnvironmentMap> Environment;
		
		// Debug options
		bool DrawGrid = false;
		bool DrawGizmos = false;
		bool DrawBounds = false;
	};

	// ============================================================================
	// RENDER PASS BASE CLASS
	// ============================================================================
	
	/**
	 * @class RenderPassBase
	 * @brief Base class for all render passes
	 */
	class RenderPassBase {
	public:
		virtual ~RenderPassBase() = default;
		
		/**
		 * @brief Get pass name
		 */
		virtual const char* GetName() const = 0;
		
		/**
		 * @brief Setup pass resources in the render graph
		 */
		virtual void Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) = 0;
		
		/**
		 * @brief Execute the render pass
		 */
		virtual void Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) = 0;
		
		/**
		 * @brief Check if pass should be executed
		 */
		virtual bool ShouldExecute(const SceneRenderInfo& sceneInfo) const { return true; }
	};

} // namespace Lunex
