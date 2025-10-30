// Renderer/Renderer3D/RendererPipeline3D.cpp
#include "stpch.h"
#include "Renderer/Renderer3D/RendererPipeline3D.h"
#include "Renderer/Renderer3D/Mesh.h"
#include "Renderer/MaterialSystem/MaterialInstance.h"

#include "Renderer/RenderCore/RenderCommand.h"
#include "Renderer/Buffer/VertexArray.h"
#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Texture.h"
#include "Renderer/Shader.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	// Minimal Light definition
	struct Light {
		enum class Type : uint32_t { Directional = 0, Point = 1, Spot = 2 };
		Type LightType = Type::Directional;
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Direction = { 0.0f, -1.0f, 0.0f };
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		float InnerCone = glm::radians(15.0f);
		float OuterCone = glm::radians(30.0f);
	};

	RendererPipeline3D::RendererPipeline3D() {
		m_Context = CreateRef<RenderContext>();
	}

	RendererPipeline3D::~RendererPipeline3D() {
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

		// Create camera UBO (binding = 0)
		// Layout: mat4 ViewProjection + vec4 CameraPosition
		m_CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4) + sizeof(glm::vec4), 0);

		// For lights: fixed size buffer (binding = 1)
		m_LightsUniformBuffer = UniformBuffer::Create(16 * 1024, 1);

		// Stats
		m_Stats = {};
	}

	void RendererPipeline3D::InitFramebuffers() {
		LNX_PROFILE_FUNCTION();

		// Main framebuffer for 3D rendering
		FramebufferSpecification spec;
		spec.Width = 1280;
		spec.Height = 720;
		spec.Attachments = { 
			FramebufferTextureFormat::RGBA8, 
			FramebufferTextureFormat::RED_INTEGER, 
			FramebufferTextureFormat::Depth 
		};
		m_Framebuffer = Framebuffer::Create(spec);

		m_Width = spec.Width;
		m_Height = spec.Height;

		// TODO: GBuffer for deferred rendering (future)
	}

	void RendererPipeline3D::InitShaders() {
		LNX_PROFILE_FUNCTION();

		// Load basic 3D shader from Lunex-Editor assets
		m_GeometryShader = Shader::Create("assets/shaders/Basic3D.glsl");

		// TODO: Shadow shader, lighting pass shader (future)
	}

	void RendererPipeline3D::InitBuffers() {
		LNX_PROFILE_FUNCTION();
		// Reserved for future use (instance buffers, etc.)
	}

	// --------------------------------------------------------------------
	// Frame control
	// --------------------------------------------------------------------
	void RendererPipeline3D::BeginFrame() {
		LNX_PROFILE_FUNCTION();

		// ? NO bind framebuffer aquí - el EditorLayer ya tiene uno bound
		// Solo habilitamos depth test para 3D
		RenderCommand::SetDepthTest(true);

		// Reset stats
		m_Stats.DrawCalls = 0;
		m_Stats.TriangleCount = 0;
	}

	void RendererPipeline3D::EndFrame() {
		LNX_PROFILE_FUNCTION();

		// Execute rendering passes
		ExecuteGeometryPass();

		// ? NO unbind framebuffer aquí - el EditorLayer lo maneja
	
		// Clear submissions for next frame
		ResetSubmissions();
	}

	// --------------------------------------------------------------------
	// Scene control
	// --------------------------------------------------------------------
	void RendererPipeline3D::BeginScene(const EditorCamera& camera, const glm::mat4& cameraTransform) {
		LNX_PROFILE_FUNCTION();

		// Upload camera data to UBO
		struct CameraUBO {
			glm::mat4 ViewProjection;
			glm::vec4 CameraPosition;
		} camUbo;

		camUbo.ViewProjection = camera.GetViewProjection();
		
		// Extract camera position from view matrix
		glm::vec3 camPos = camera.GetPosition();
		camUbo.CameraPosition = glm::vec4(camPos, 1.0f);

		if (m_CameraUniformBuffer) {
			m_CameraUniformBuffer->SetData(&camUbo, sizeof(CameraUBO));
		}
	}

	// ? Generic camera overload for SceneCamera in Runtime
	void RendererPipeline3D::BeginScene(const Camera& camera, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition) {
		LNX_PROFILE_FUNCTION();

		// Upload camera data to UBO
		struct CameraUBO {
			glm::mat4 ViewProjection;
			glm::vec4 CameraPosition;
		} camUbo;

		camUbo.ViewProjection = camera.GetProjection() * viewMatrix;
		camUbo.CameraPosition = glm::vec4(cameraPosition, 1.0f);

		if (m_CameraUniformBuffer) {
			m_CameraUniformBuffer->SetData(&camUbo, sizeof(CameraUBO));
		}

		LNX_LOG_INFO("BeginScene with SceneCamera: ViewProj uploaded, Position=({0}, {1}, {2})", 
			cameraPosition.x, cameraPosition.y, cameraPosition.z);
	}

	void RendererPipeline3D::EndScene() {
		LNX_PROFILE_FUNCTION();
		
		// Execute all rendering
		EndFrame();
	}

	// --------------------------------------------------------------------
	// Submissions
	// --------------------------------------------------------------------
	void RendererPipeline3D::Submit(const Ref<VertexArray>& vertexArray, const Ref<Shader>& shader, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();

		shader->Bind();
		shader->SetMat4("u_Transform", transform);
		
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
		
		m_Stats.DrawCalls++;
		
		Ref<IndexBuffer> ib = vertexArray->GetIndexBuffer();
		if (ib) {
			m_Stats.TriangleCount += (ib->GetCount() / 3);
		}
	}

	void RendererPipeline3D::SubmitMesh(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform, uint32_t entityID) {
		LNX_PROFILE_FUNCTION();

		if (!mesh || !material) return;

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

		// Debug log
		static bool logged = false;
		if (!logged) {
			LNX_LOG_INFO("ExecuteGeometryPass() - Rendering {0} mesh submissions", m_MeshSubmissions.size());
			logged = true;
		}

		// Render all submitted meshes
		for (auto& submission : m_MeshSubmissions) {
			// Bind material (sets shader + uniforms)
			if (submission.Material) {
				submission.Material->Bind();
				
				// Set per-object uniforms via Transform UBO (binding = 1)
				Ref<Shader> shader = submission.Material->GetShader();
				if (shader) {
					// Calculate normal matrix
					glm::mat3 normalMatrix3 = glm::transpose(glm::inverse(glm::mat3(submission.Transform)));
					glm::mat4 normalMatrix4 = glm::mat4(
						glm::vec4(normalMatrix3[0], 0.0f),
						glm::vec4(normalMatrix3[1], 0.0f),
						glm::vec4(normalMatrix3[2], 0.0f),
						glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
					);

					// Upload Transform UBO data
					// TODO: Use actual UBO instead of individual uniforms
					shader->SetMat4("u_Transform", submission.Transform);
					shader->SetMat4("u_NormalMatrix", normalMatrix4);
					shader->SetInt("u_EntityID", (int)submission.EntityID);
				}
			}

			// Draw mesh
			if (submission.Mesh) {
				Ref<VertexArray> va = submission.Mesh->GetVertexArray();
				if (va) {
					va->Bind();
					RenderCommand::DrawIndexed(va);
					
					m_Stats.DrawCalls++;
					m_Stats.TriangleCount += (submission.Mesh->GetIndexCount() / 3);
				}

				if (submission.Material) {
					submission.Material->Unbind();
				}
			}
		}
	}

	// --------------------------------------------------------------------
	// Stats
	// --------------------------------------------------------------------
	RendererPipeline3D::Statistics RendererPipeline3D::GetStats() const {
		return m_Stats;
	}
}