// Renderer/Renderer3D/RendererPipeline3D.cpp
#include "stpch.h"
#include "Renderer/Renderer3D/RendererPipeline3D.h"

#include "Renderer/RenderCore/RenderCommand.h"
#include "Renderer/Buffer/VertexArray.h"
#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Texture.h"
#include "Renderer/Shader.h"

// If Mesh/Material/Light implementations live elsewhere, include their headers
// #include "Renderer/Renderer3D/Assets/Mesh.h"
// #include "Renderer/Renderer3D/Assets/Material.h"

namespace Lunex {

	// Minimal Light definition (if you have your own, remove this and include it)
	struct Light {
		enum class Type : uint32_t { Directional = 0, Point = 1, Spot = 2 };
		Type Type = Type::Directional;
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Direction = { 0.0f, -1.0f, 0.0f };
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		// spot params
		float InnerCone = glm::radians(15.0f);
		float OuterCone = glm::radians(30.0f);
	};

	RendererPipeline3D::RendererPipeline3D() {
		m_Context = CreateRef<RenderContext>(); // placeholder: depending on how you create contexts
	}

	RendererPipeline3D::~RendererPipeline3D() {
		// Resources are Ref<>, so auto-clean; explicit reset optional
		ResetSubmissions();
	}

	// --------------------------------------------------------------------
	// Init / Shutdown
	// --------------------------------------------------------------------
	void RendererPipeline3D::Init() {
		LNX_PROFILE_FUNCTION();

		InitFramebuffers();
		InitShaders();
		InitBuffers();

		// Create camera & lights UBOs
		m_CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4) * 2 + sizeof(glm::vec4), 0); // simple: VP + view + camPos, binding 0
		// For lights: choose a conservative fixed size or use SSBO later
		m_LightsUniformBuffer = UniformBuffer::Create(16 * 1024, 1); // placeholder size, binding = 1

		// Stats
		m_Stats = {};
	}

	void RendererPipeline3D::InitFramebuffers() {
		LNX_PROFILE_FUNCTION();

		// Create a simple framebuffer for the pipeline (editor viewport). If you have your own Framebuffer class, use it.
		FramebufferSpecification spec;
		spec.Width = 1280;
		spec.Height = 720;
		spec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth }; // placeholder - adjust to your FramebufferSpecification
		m_Framebuffer = Framebuffer::Create(spec);

		// Store dimensions for later use
		m_Width = spec.Width;
		m_Height = spec.Height;

		// GBuffer (if deferred)
		if (m_UseDeferred) {
			FramebufferSpecification gspec;
			gspec.Width = spec.Width;
			gspec.Height = spec.Height;
			gspec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth }; // example
			m_GBuffer = Framebuffer::Create(gspec);

			// Create GBuffer textures (these APIs depend on your Texture2D creation)
			m_GPosition = Texture2D::Create(spec.Width, spec.Height);
			m_GNormal = Texture2D::Create(spec.Width, spec.Height);
			m_GAlbedo = Texture2D::Create(spec.Width, spec.Height);
			// Note: you will likely need to attach these to the framebuffer — leave for your Framebuffer::Create/Attach APIs
		}
	}

	void RendererPipeline3D::InitShaders() {
		LNX_PROFILE_FUNCTION();

		// Load / create shaders. Use your Shader::Create or ResourceManager if you have one.
		// These shader paths are placeholders; create shaders in assets/shaders/3d/
		m_GeometryShader = Shader::Create("assets/shaders/3d/gbuffer.glsl");    // writes position/normal/albedo
		m_LightingShader = Shader::Create("assets/shaders/3d/lighting_pass.glsl");
		m_ShadowShader = Shader::Create("assets/shaders/3d/shadow_depth.glsl");

		// If your shader needs sampler arrays or other uniforms setup, set them here.
		if (m_LightingShader) {
			m_LightingShader->Bind();
			// example: set texture slots for GBuffer
			int ids[4] = { 0, 1, 2, 3 };
			m_LightingShader->SetIntArray("u_GBufferTextures", ids, 3);
		}
	}

	void RendererPipeline3D::InitBuffers() {
		LNX_PROFILE_FUNCTION();
		// If you need global vertex buffers, instance buffers, or other GPU buffers, create them here.
		// For now we rely on per-mesh VertexArray/IndexBuffer inside Mesh asset.
	}

	// --------------------------------------------------------------------
	// Frame control
	// --------------------------------------------------------------------
	void RendererPipeline3D::BeginFrame() {
		LNX_PROFILE_FUNCTION();

		// Bind the pipeline framebuffer (for editor viewport), clear
		if (m_Framebuffer) {
			m_Framebuffer->Bind();
			RenderCommand::SetViewport(0, 0, m_Width, m_Height);
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.12f, 1.0f });
			RenderCommand::Clear();
		}
		else {
			// fallback: clear default framebuffer
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.12f, 1.0f });
			RenderCommand::Clear();
		}

		// reset stats for the frame
		m_Stats.DrawCalls = 0;
		m_Stats.TriangleCount = 0;
	}

	void RendererPipeline3D::EndFrame() {
		LNX_PROFILE_FUNCTION();

		// Execute pipeline passes
		if (m_UseDeferred) {
			ExecuteGeometryPass();
			ExecuteShadowPass();
			ExecuteLightingPass();
		}
		else {
			// forward path: geometry pass will perform lighting within the shader
			ExecuteGeometryPass();
			ExecutePostProcess();
		}

		// Unbind framebuffer if used
		if (m_Framebuffer) {
			m_Framebuffer->Unbind();
		}

		// Swap buffers via context if available
		if (m_Context) {
			// If your RenderContext exposes SwapBuffers(void* window) or similar, call it here.
			// m_Context->SwapBuffers();
		}

		// Clear submissions for next frame
		ResetSubmissions();
	}

	// --------------------------------------------------------------------
	// Scene control
	// --------------------------------------------------------------------
	void RendererPipeline3D::BeginScene(const EditorCamera& camera, const glm::mat4& cameraTransform) {
		LNX_PROFILE_FUNCTION();

		// Upload camera matrices to UBO
		// We'll store ViewProjection in CameraBuffer.ViewProjection (as defined in header)
		// Here we do a simple upload: the shader expects "u_ViewProjection" uniform (or UBO)
		glm::mat4 viewProj = camera.GetProjection() * glm::inverse(cameraTransform);
		if (m_CameraUniformBuffer) {
			// You must decide layout of camera UBO on shader side; here we upload viewProj and camera position
			struct CameraUBO {
				glm::mat4 ViewProjection;
				glm::vec4 CameraPosition; // pad vec3 to vec4
			} camUbo;
			camUbo.ViewProjection = viewProj;
			// extract camera position from transform (inverted)
			glm::mat4 inv = glm::inverse(cameraTransform);
			glm::vec3 camPos = { inv[3][0], inv[3][1], inv[3][2] };
			camUbo.CameraPosition = glm::vec4(camPos, 1.0f);

			m_CameraUniformBuffer->SetData(&camUbo, sizeof(CameraUBO));
		}

		// store camera in PipelineData for use during submit/flush
	some_unused_variable_warning_workaround:; // placeholder to avoid unused warning if needed
	}

	void RendererPipeline3D::EndScene() {
		LNX_PROFILE_FUNCTION();
		// nothing extra here; EndFrame will execute passes
	}

	// --------------------------------------------------------------------
	// Submissions
	// --------------------------------------------------------------------
	void RendererPipeline3D::Submit(const Ref<VertexArray>& vertexArray, const Ref<Shader>& shader, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();
		// Simple submit: draw immediately (useful for quick debug draws)
		shader->Bind();
		shader->SetMat4("u_Transform", transform);
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
		m_Stats.DrawCalls++;
		// If you can query index count, add triangle count:
		Ref<IndexBuffer> ib = vertexArray->GetIndexBuffer();
		if (ib) m_Stats.TriangleCount += (ib->GetCount() / 3);
	}

	void RendererPipeline3D::SubmitMesh(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform, uint32_t entityID) {
		LNX_PROFILE_FUNCTION();

		MeshSubmission s;
		s.Mesh = mesh;
		s.Material = material;
		s.Transform = transform;
		s.EntityID = entityID;
		m_MeshSubmissions.push_back(std::move(s));
	}

	void RendererPipeline3D::SubmitLight(const Light& light) {
		LNX_PROFILE_FUNCTION();
		m_Lights.push_back(light);
	}

	void RendererPipeline3D::ResetSubmissions() {
		m_MeshSubmissions.clear();
		m_Lights.clear();
	}

	// --------------------------------------------------------------------
	// Passes
	// --------------------------------------------------------------------
	void RendererPipeline3D::ExecuteGeometryPass() {
		LNX_PROFILE_FUNCTION();

		// If deferred: write to GBuffer; if forward: render shaded
		if (m_UseDeferred) {
			if (m_GBuffer)
				m_GBuffer->Bind();

			// Clear GBuffer attachments if needed
			RenderCommand::Clear();

			// For each mesh submission, bind geometry shader / material and draw
			for (auto& s : m_MeshSubmissions) {
				// Material should provide shader and set material-specific uniforms/textures
				Ref<Shader> shader = nullptr;
				if (s.Material) {
					// assume Material has GetShader() and Bind() methods
					// s.Material->Bind(); // may set textures/uniforms
					// shader = s.Material->GetShader();
					// Fallback: try to get a shader from material, else use geometry shader
					// (Change this to your Material API)
				}
				if (!shader) shader = m_GeometryShader;

				if (shader) {
					shader->Bind();
					// set per-object transform
					shader->SetMat4("u_Model", s.Transform);
					// set other per-object material uniforms if needed
				}

				// Draw mesh (assume Mesh exposes GetVertexArray())
				if (s.Mesh) {
					// Ref<VertexArray> va = s.Mesh->GetVertexArray(); // adjust to your Mesh API
					// if (va) {
					//     va->Bind();
					//     RenderCommand::DrawIndexed(va);
					//     // Stats
					//     Ref<IndexBuffer> ib = va->GetIndexBuffer();
					//     if (ib) m_Stats.TriangleCount += (ib->GetCount() / 3);
					//     m_Stats.DrawCalls++;
					// }
					// Placeholder: skip drawing until Mesh API is implemented
				}
			}

			if (m_GBuffer)
				m_GBuffer->Unbind();
		}
		else {
			// Forward rendering: use geometry shader that performs lighting per object (cheap)
			for (auto& s : m_MeshSubmissions) {
				Ref<Shader> shader = m_GeometryShader; // (s.Material) ? s.Material->GetShader() : m_GeometryShader; // adjust to your Material API
				if (!shader) shader = m_GeometryShader;
				shader->Bind();
				shader->SetMat4("u_Model", s.Transform);
				// set camera UBO if shader wants it (some shaders read u_ViewProjection from UBO)
				if (s.Mesh) {
					// Ref<VertexArray> va = s.Mesh->GetVertexArray();
					// if (va) {
					//     va->Bind();
					//     RenderCommand::DrawIndexed(va);
					//     Ref<IndexBuffer> ib = va->GetIndexBuffer();
					//     if (ib) m_Stats.TriangleCount += (ib->GetCount() / 3);
					//     m_Stats.DrawCalls++;
					// }
					// Placeholder: skip drawing until Mesh API is implemented
				}
			}
		}
	}

	void RendererPipeline3D::ExecuteShadowPass() {
		LNX_PROFILE_FUNCTION();

		// Simple stub: if shadows are required, you would:
		//  - for each shadow-casting light, bind shadow framebuffer,
		//  - set viewport to shadowmap size, clear depth,
		//  - render each shadow-casting mesh with depth-only shader (m_ShadowShader),
		//  - store shadowmaps for lighting pass.
		// For now we leave it minimal.

		if (!m_ShadowShader)
			return;

		// Example (pseudocode):
		// for (auto& light : m_Lights) {
		//     if (!light.castsShadows) continue;
		//     Setup shadow framebuffer for this light
		//     m_ShadowShader->Bind();
		//     for (auto& s : m_MeshSubmissions) {
		//         m_ShadowShader->SetMat4("u_Model", s.Transform);
		//         s.Mesh->GetVertexArray()->Bind();
		//         RenderCommand::DrawIndexed(s.Mesh->GetVertexArray());
		//     }
		// }

	}

	void RendererPipeline3D::ExecuteLightingPass() {
		LNX_PROFILE_FUNCTION();

		// Deferred lighting: read GBuffer textures and apply lighting in a fullscreen pass
		if (m_UseDeferred) {
			if (!m_LightingShader) return;

			// Bind default framebuffer or pipeline framebuffer to write lit result
			if (m_Framebuffer) m_Framebuffer->Bind();

			m_LightingShader->Bind();
			// bind GBuffer textures to slots (assumes you created them)
			if (m_GPosition) m_GPosition->Bind(0);
			if (m_GNormal) m_GNormal->Bind(1);
			if (m_GAlbedo) m_GAlbedo->Bind(2);

			// Upload lights to UBO/SSBO
			// Here we use a very simple approach: pack a small set of lights in the lights UBO
			// The shader must be prepared to read a uniform block or SSBO with the same layout.
			// For simplicity, if you haven't implemented SSBOs, you can upload a few lights as arrays of vec4 via SetFloatVec/SetVec3 calls.

			// Example pseudo:
			// m_LightsUniformBuffer->SetData(m_Lights.data(), sizeof(Light) * m_Lights.size());

			// Draw fullscreen triangle/quad to apply lighting
			// For simplicity we assume there's a helper full-screen VA in RenderCore; if not, you need to create one.
			// RenderCommand::DrawFullscreenTriangle(); // pseudo
			// Fallback: we reuse a simple screen-aligned quad VertexArray if you have it

			// Unbind framebuffer if needed
			if (m_Framebuffer) m_Framebuffer->Unbind();
		}
		else {
			// Forward path: lighting already executed in geometry pass
		}
	}
	
	// --------------------------------------------------------------------
	// Stats
	// --------------------------------------------------------------------
	RendererPipeline3D::Statistics RendererPipeline3D::GetStats() const {
		return m_Stats;
	}
}