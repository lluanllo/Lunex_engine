#pragma once

/**
 * @file DeferredRenderer.h
 * @brief Deferred Rendering Pipeline
 * 
 * Two-pass rendering:
 *   1. Geometry Pass:  Render all meshes into G-Buffer (MRT)
 *   2. Lighting Pass:  Full-screen quad reads G-Buffer, computes PBR lighting
 * 
 * The lighting pass produces the final HDR image into the scene framebuffer,
 * where tone mapping and gamma correction are applied.
 * 
 * This renderer respects the RHI abstraction - no raw graphics API calls.
 */

#include "Core/Core.h"
#include "Renderer/Deferred/GBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/EnvironmentMap.h"
#include "Scene/Camera/EditorCamera.h"

namespace Lunex {

	// Forward declarations
	struct MeshComponent;
	struct MaterialComponent;
	struct TextureComponent;
	class Scene;

	/**
	 * @class DeferredRenderer
	 * @brief Orchestrates deferred rendering: Geometry Pass + Lighting Pass
	 */
	class DeferredRenderer {
	public:
		static void Init();
		static void Shutdown();

		/**
		 * @brief Check if deferred rendering is enabled
		 */
		static bool IsEnabled();
		static void SetEnabled(bool enabled);

		// ========== SCENE MANAGEMENT ==========

		static void BeginScene(const EditorCamera& camera);
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void EndScene();

		// ========== GEOMETRY PASS ==========

		/**
		 * @brief Begin the geometry pass (binds G-Buffer)
		 */
		static void BeginGeometryPass();

		/**
		 * @brief End the geometry pass (unbinds G-Buffer)
		 */
		static void EndGeometryPass();

		/**
		 * @brief Submit a mesh with material for rendering into G-Buffer
		 */
		static void SubmitMesh(const glm::mat4& transform, MeshComponent& meshComponent, 
							   MaterialComponent& materialComponent, int entityID = -1);

		/**
		 * @brief Submit a mesh without material (default material)
		 */
		static void SubmitMesh(const glm::mat4& transform, MeshComponent& meshComponent, 
							   int entityID = -1);

		// ========== LIGHTING PASS ==========

		/**
		 * @brief Execute the lighting pass (reads G-Buffer, writes to target framebuffer)
		 * @param targetFramebuffer The framebuffer to write the final lit image to (scene FB)
		 */
		static void ExecuteLightingPass(const Ref<Framebuffer>& targetFramebuffer);

		// ========== ENVIRONMENT ==========

		static void BindEnvironment(const Ref<EnvironmentMap>& environment);
		static void UnbindEnvironment();

		// ========== G-BUFFER ACCESS ==========

		/**
		 * @brief Get the G-Buffer (for debug visualization, picking, etc.)
		 */
		static GBuffer& GetGBuffer();

		/**
		 * @brief Resize the G-Buffer when viewport changes
		 */
		static void OnViewportResize(uint32_t width, uint32_t height);

		/**
		 * @brief Read entity ID from G-Buffer at pixel position
		 */
		static int ReadEntityID(int x, int y);

		// ========== STATISTICS ==========

		struct Statistics {
			uint32_t GeometryDrawCalls = 0;
			uint32_t LightingDrawCalls = 0;
			uint32_t MeshCount = 0;
			uint32_t TriangleCount = 0;
		};

		static void ResetStats();
		static Statistics GetStats();
	};

} // namespace Lunex
