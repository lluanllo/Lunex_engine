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
		enum class Type : uint32_t { Directional =0, Point =1, Spot =2 };
		Type LightType = Type::Directional;
		glm::vec3 Position = {0.0f,0.0f,0.0f };
		glm::vec3 Direction = {0.0f, -1.0f,0.0f };
		glm::vec3 Color = {1.0f,1.0f,1.0f };
		float Intensity =1.0f;
		float Range =10.0f;
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

		// Create camera UBO (binding =4)
		// Layout: mat4 ViewProjection + vec4 CameraPosition
		m_CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4) + sizeof(glm::vec4),4);

		// Create transform UBO (binding =5)
		// Layout: mat4 Transform + mat4 NormalMatrix + int EntityID + padding
		struct TransformUBO {
			glm::mat4 Transform;
			glm::mat4 NormalMatrix;
			int EntityID;
			glm::vec3 _padding; // Align to16 bytes
		};
		m_TransformUniformBuffer = UniformBuffer::Create(sizeof(TransformUBO),5);

		// Create material UBO (binding =6)
		// Layout matches shader: vec4 Albedo_Metallic + vec4 Roughness_Emission + vec4 Flags
		m_MaterialUniformBuffer = UniformBuffer::Create(sizeof(glm::vec4) *3,6);

		// For lights: fixed size buffer (binding =7 to avoid conflict)
		m_LightsUniformBuffer = UniformBuffer::Create(16 *1024,7);

		// Stats
		m_Stats = {};
	}

	void RendererPipeline3D::InitFramebuffers() {
		LNX_PROFILE_FUNCTION();

		// Main framebuffer for3D rendering
		FramebufferSpecification spec;
		spec.Width =1280;
		spec.Height =720;
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

		// Load basic3D shader from Lunex-Editor assets
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

		// Enable depth test for3D
		RenderCommand::SetDepthTest(true);

		// Reset stats
		m_Stats.DrawCalls =0;
		m_Stats.TriangleCount =0;
	}

	void RendererPipeline3D::EndFrame() {
		LNX_PROFILE_FUNCTION();

		// Execute rendering passes
		Flush();

		// Do not toggle global states here. Editor manages framebuffer.
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
		glm::vec3 camPos = camera.GetPosition();
		camUbo.CameraPosition = glm::vec4(camPos,1.0f);

		if (m_CameraUniformBuffer) {
			m_CameraUniformBuffer->SetData(&camUbo, sizeof(CameraUBO));
		}

		StartBatch();
	}

	void RendererPipeline3D::BeginScene(const Camera& camera, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition) {
		LNX_PROFILE_FUNCTION();

		struct CameraUBO {
			glm::mat4 ViewProjection;
			glm::vec4 CameraPosition;
		} camUbo;

		camUbo.ViewProjection = camera.GetProjection() * viewMatrix;
		camUbo.CameraPosition = glm::vec4(cameraPosition,1.0f);

		if (m_CameraUniformBuffer) {
			m_CameraUniformBuffer->SetData(&camUbo, sizeof(CameraUBO));
		}

		StartBatch();
	}

	void RendererPipeline3D::EndScene() {
		LNX_PROFILE_FUNCTION();
		EndFrame();
	}

	// --------------------------------------------------------------------
	// Submissions (BATCH MODE)
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
			m_Stats.TriangleCount += (ib->GetCount() /3);
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
	// Batch Management (following Renderer2D pattern)
	// --------------------------------------------------------------------
	void RendererPipeline3D::StartBatch() {
		m_MeshSubmissions.clear();
		m_Lights.clear();
	}

	void RendererPipeline3D::NextBatch() {
		Flush();
		StartBatch();
	}

	void RendererPipeline3D::Flush() {
		LNX_PROFILE_FUNCTION();

		if (m_MeshSubmissions.empty()) return;

		Ref<Shader> lastShader = nullptr;
		Ref<Material> lastMaterial = nullptr;

		for (auto& submission : m_MeshSubmissions) {
			if (submission.Material != lastMaterial) {
				if (lastMaterial) {
					lastMaterial->Unbind();
				}
				submission.Material->Bind();
				lastMaterial = submission.Material;
				lastShader = submission.Material->GetShader();
				
				// Upload Material to UBO (binding =6)
				struct MaterialUBO {
					glm::vec4 Albedo_Metallic;
					glm::vec4 Roughness_Emission_X;
					glm::vec4 Flags;
				} materialUbo;
				
				materialUbo.Albedo_Metallic = glm::vec4(submission.Material->GetAlbedo(), submission.Material->GetMetallic());
				materialUbo.Roughness_Emission_X = glm::vec4(submission.Material->GetRoughness(), submission.Material->GetEmission());
				materialUbo.Flags = glm::vec4(0.0f);
				
				if (m_MaterialUniformBuffer) {
					m_MaterialUniformBuffer->SetData(&materialUbo, sizeof(MaterialUBO));
				}
			}

			if (lastShader) {
				// Upload Transform UBO data (binding =5)
				struct TransformUBO {
					glm::mat4 Transform;
					glm::mat4 NormalMatrix;
					int EntityID;
					glm::vec3 _padding;
				} transformUbo;

				glm::mat3 normalMatrix3 = glm::transpose(glm::inverse(glm::mat3(submission.Transform)));
				glm::mat4 normalMatrix4 = glm::mat4(
					glm::vec4(normalMatrix3[0],0.0f),
					glm::vec4(normalMatrix3[1],0.0f),
					glm::vec4(normalMatrix3[2],0.0f),
					glm::vec4(0.0f,0.0f,0.0f,1.0f)
				);

				transformUbo.Transform = submission.Transform;
				transformUbo.NormalMatrix = normalMatrix4;
				transformUbo.EntityID = (int)submission.EntityID;

				if (m_TransformUniformBuffer) {
					m_TransformUniformBuffer->SetData(&transformUbo, sizeof(TransformUBO));
				}
			}

			if (submission.Mesh) {
				Ref<VertexArray> va = submission.Mesh->GetVertexArray();
				if (va) {
					va->Bind();
					RenderCommand::DrawIndexed(va);
					
					m_Stats.DrawCalls++;
					m_Stats.TriangleCount += (submission.Mesh->GetIndexCount() /3);
				}
			}
		}

		if (lastMaterial) {
			lastMaterial->Unbind();
		}

		m_MeshSubmissions.clear();
	}

	// --------------------------------------------------------------------
	// Stats
	// --------------------------------------------------------------------
	RendererPipeline3D::Statistics RendererPipeline3D::GetStats() const {
		return m_Stats;
	}
}