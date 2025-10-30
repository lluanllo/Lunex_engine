#pragma once
#include "Renderer/RenderCore/RenderPipeline.h"

#include "Renderer/Buffer/VertexArray.h"
#include "Renderer/Buffer/Framebuffer.h"
#include "Renderer/Buffer/UniformBuffer.h"

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include "Renderer/CameraTypes/EditorCamera.h"

#include "Renderer/RenderCore/RenderContext.h"
#include "Renderer/RenderCore/RenderPass.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Lunex {
	// Forward-declare asset types (implementaciones concretas en Renderer3D/Assets)
	class Mesh;        // wrapper asset: vertex arrays, bounds, source path, UUID
	class Material;    // wraps Shader + uniform bindings
	struct Light;      // simple light descriptor (to be defined)
	
	/**
	 * RendererPipeline3D
	 *
	 * Pipeline 3D principal. Encapsula:
	 *  - inicialización de recursos 3D (framebuffers, shaders base, g-buffer si lo usas)
	 *  - ciclo frame: BeginFrame / EndFrame
	 *  - control de escena: BeginScene(camera), EndScene()
	 *  - submit de objetos (mesh + material + transform)
	 *  - gestión de luces (colección de lights por UBO/SSBO)
	 *
	 * Implementa RenderPipeline (interfaz común para todos los pipelines).
	 */
	class RendererPipeline3D : public RenderPipeline {
		public:
			RendererPipeline3D();
			virtual ~RendererPipeline3D();
			
			// ----- RenderPipeline interface -----
			void Init() override;
			void BeginFrame() override;
			void EndFrame() override;
			
			// Basic Submit (used by Scene / RenderSystem3D)
			void Submit(const Ref<VertexArray>& vertexArray, const Ref<Shader>& shader, const glm::mat4& transform = glm::mat4(1.0f)) override;
			
			// Optional: access to framebuffer (for editor viewport, postprocess, etc.)
			Ref<Framebuffer> GetFramebuffer() const override { return m_Framebuffer; }
			
			const std::string& GetName() const override { return m_Name; }
			
			// ----- Scene control (3D-specific) -----
			// Provide camera and per-frame data
			void BeginScene(const EditorCamera& camera, const glm::mat4& cameraTransform);
			void BeginScene(const Camera& camera, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition); // ? Generic camera
			void EndScene();
			
			// Submit high-level objects: Mesh + Material
			void SubmitMesh(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform, uint32_t entityID = 0);
			
			// Lights: basic API to accumulate lights for lighting pass.
			// Light struct is engine-specific (position, color, intensity, type, direction, range, spot params...)
			void SubmitLight(const Light& light);
			
			// Clear submitted data from last frame without destroying pipeline resources
			void ResetSubmissions();
			
			// Pipeline statistics (simple)
			struct Statistics {
				uint32_t DrawCalls = 0;
				uint32_t TriangleCount = 0;
				// extend as needed: passes executed, shadowmaps, etc.
			};
			Statistics GetStats() const;
			
		private:
			// Internal helpers for pipeline setup and passes
			void InitFramebuffers();
			void InitShaders();
			void InitBuffers();
			
			void ExecuteGeometryPass();   // fill G-buffer or forward geometry
			void ExecuteShadowPass();     // render shadow maps for lights that cast shadows
			void ExecuteLightingPass();   // perform lighting (deferred or forward)
			void ExecutePostProcess();    // tonemapping, FXAA, bloom, etc.
			
			// Upload per-frame UBOs: camera, lights, material parameters
			void UploadPerFrameData();
			
		private:
			std::string m_Name = "RendererPipeline3D";
			
			// Core resources
			Ref<RenderContext> m_Context;     // reference to the render context (swapbuffers, API)
			Ref<Framebuffer> m_Framebuffer;   // main pipeline framebuffer (editor viewport)
			
			// Shaders
			Ref<Shader> m_GeometryShader;     // if forward: base shader; if deferred: gbuffer writer
			Ref<Shader> m_LightingShader;     // lighting pass (deferred) or forward lighting helper
			Ref<Shader> m_ShadowShader;       // depth-only shader for shadow maps
			
			// Optional resources for deferred pipeline
			Ref<Framebuffer> m_GBuffer;       // position/normals/albedo/metallic/roughness/ao
			Ref<Texture2D> m_GPosition;
			Ref<Texture2D> m_GNormal;
			Ref<Texture2D> m_GAlbedo;
			// ... add as needed
			
			// Per-frame UBOs
			Ref<UniformBuffer> m_CameraUniformBuffer;
			Ref<UniformBuffer> m_LightsUniformBuffer;
			
			// Submissions recorded this frame
			struct MeshSubmission {
				Ref<Mesh> Mesh;
				Ref<Material> Material;
				glm::mat4 Transform;
				uint32_t EntityID;
			};
			std::vector<MeshSubmission> m_MeshSubmissions;
			
			std::vector<Light> m_Lights;
			
			// Stats
			Statistics m_Stats;
			
			// Config flags
			bool m_UseDeferred = true; // default: use deferred pipeline (changeable later)

			// Framebuffer dimensions
			uint32_t m_Width = 1280;
			uint32_t m_Height = 720;
	};
}