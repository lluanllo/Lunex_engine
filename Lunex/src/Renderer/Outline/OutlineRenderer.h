#pragma once

/**
 * @file OutlineRenderer.h
 * @brief Post-process outline system for selection highlights and collider visualization
 *
 * Uses a blurred-buffer technique:
 *   1. Render selected objects / collider shapes as flat-color silhouettes
 *   2. Two-pass separable blur to expand the silhouette
 *   3. Composite the outline onto the scene framebuffer
 */

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "RHI/RHI.h"

#include <glm/glm.hpp>
#include <vector>
#include <set>

namespace Lunex {

	class Scene;
	class Entity;
	class EditorCamera;
	class Camera;

	class OutlineRenderer {
	public:
		static OutlineRenderer& Get();

		void Initialize(uint32_t width, uint32_t height);
		void Shutdown();
		void OnViewportResize(uint32_t width, uint32_t height);

		// ====================================================================
		// MAIN ENTRY POINTS
		// ====================================================================

		/**
		 * Render outline for a set of selected entities (meshes + sprites).
		 * Performs a complete silhouette?blur?composite cycle onto targetFBOHandle.
		 */
		void RenderSelectionOutline(
			Scene* scene,
			const std::set<Entity>& selectedEntities,
			const glm::mat4& viewProjection,
			uint64_t targetFBOHandle,
			const glm::vec4& outlineColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)
		);

		/**
		 * Render collider debug shapes as outlined silhouettes.
		 * Performs a complete silhouette?blur?composite cycle onto targetFBOHandle.
		 */
		void RenderColliderOutlines(
			Scene* scene,
			const glm::mat4& viewProjection,
			uint64_t targetFBOHandle,
			bool show3D,
			bool show2D,
			const glm::vec4& collider3DColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
			const glm::vec4& collider2DColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
		);

		/**
		 * Execute the blur + composite passes and blend onto targetFBOHandle.
		 * @deprecated Each render pass now composites internally. Kept for compatibility.
		 */
		void Composite(uint64_t targetFBOHandle);

		// ====================================================================
		// CONFIGURATION
		// ====================================================================

		struct Config {
			int   KernelSize      = 3;      // Blur radius (outline width)
			float OutlineHardness = 0.75f;  // 0 = soft/glow, 1 = hard edge
			bool  ShowBehindObjects = true;  // Outline visible through geometry
			float InsideAlpha     = 0.0f;   // 0 = transparent inside
		};

		Config& GetConfig() { return m_Config; }
		const Config& GetConfig() const { return m_Config; }

		bool IsInitialized() const { return m_Initialized; }

	private:
		OutlineRenderer() = default;
		~OutlineRenderer() = default;
		OutlineRenderer(const OutlineRenderer&) = delete;
		OutlineRenderer& operator=(const OutlineRenderer&) = delete;

		// Internal passes
		void BeginSilhouettePass();
		void EndSilhouettePass();
		void DrawEntitySilhouette(Scene* scene, Entity entity, const glm::mat4& viewProjection, const glm::vec4& color);
		void DrawColliderSilhouette(const glm::mat4& viewProjection, const glm::mat4& model, const glm::vec4& color);

		void BlurPass();
		void CompositePass(uint64_t targetFBOHandle, const glm::vec4& outlineColor);
		void DrawFullscreenQuad();

		void CreateResources();
		void CreateDebugMeshes();

	private:
		Config m_Config;
		bool m_Initialized = false;
		bool m_SilhouetteHasContent = false;

		uint32_t m_Width  = 0;
		uint32_t m_Height = 0;

		// GPU resources
		Ref<RHI::RHIFramebuffer> m_SilhouetteFBO;  // R8 + Depth
		Ref<RHI::RHIFramebuffer> m_BlurPingFBO;     // R8 (horizontal blur)
		Ref<RHI::RHIFramebuffer> m_BlurPongFBO;     // R8 (vertical blur)

		// Fullscreen quad
		Ref<VertexArray>  m_FullscreenQuadVA;
		Ref<VertexBuffer> m_FullscreenQuadVB;
		Ref<IndexBuffer>  m_FullscreenQuadIB;

		// Debug meshes for colliders
		Ref<VertexArray>  m_BoxVA;
		Ref<IndexBuffer>  m_BoxIB;
		uint32_t          m_BoxIndexCount = 0;

		Ref<VertexArray>  m_SphereVA;
		Ref<IndexBuffer>  m_SphereIB;
		uint32_t          m_SphereIndexCount = 0;

		// Shaders
		Ref<Shader> m_SilhouetteShader;
		Ref<Shader> m_BlurShader;
		Ref<Shader> m_CompositeShader;

		// UBO for silhouette shader (binding = 8)
		struct alignas(16) SilhouetteUBOData {
			glm::mat4 ViewProjection;
			glm::mat4 Model;
			glm::vec4 Color;
		};
		Ref<UniformBuffer>  m_SilhouetteUBO;
		SilhouetteUBOData   m_SilhouetteUBOData{};

		// UBO for blur shader (binding = 9)
		struct alignas(16) BlurUBOData {
			glm::vec2 TexelSize;    // 1.0 / textureSize
			glm::vec2 Direction;    // (1,0) or (0,1)
			int KernelSize;         // blur radius
			float _pad[3];
		};
		Ref<UniformBuffer>  m_BlurUBO;
		BlurUBOData         m_BlurUBOData{};

		// UBO for composite shader (binding = 10)
		struct alignas(16) CompositeUBOData {
			glm::vec4 OutlineColor;
			float OutlineHardness;
			float InsideAlpha;
			float _pad[2];
		};
		Ref<UniformBuffer>  m_CompositeUBO;
		CompositeUBOData    m_CompositeUBOData{};

		// Outline color used in the composite pass (stored per frame)
		glm::vec4 m_CurrentOutlineColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
	};

} // namespace Lunex
