#include "stpch.h"
#include "SelectionOutlinePass.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/UniformBuffer.h"
#include "Scene/Components.h"
#include "glad/glad.h"

namespace Lunex {
	// Uniform buffer structures matching shader layouts
	struct BlurParamsData {
		glm::vec2 Direction;
		float BlurRadius;
		float _padding1;
		glm::vec2 TexelSize;
		float _padding2;
		float _padding3;
	};

	struct CompositeParamsData {
		glm::vec4 OutlineColor;
		int BlendMode;
		int DepthTest;
		float _padding1;
		float _padding2;
	};

	SelectionOutlinePass::SelectionOutlinePass() {
	}

	SelectionOutlinePass::~SelectionOutlinePass() {
		Shutdown();
	}

	void SelectionOutlinePass::Init(uint32_t width, uint32_t height) {
		m_Width = width;
		m_Height = height;

		// Load shaders
		m_MaskShader = Shader::Create("assets/shaders/PostProcess/SelectionMask.glsl");
		m_BlurShader = Shader::Create("assets/shaders/PostProcess/GaussianBlur.glsl");
		m_EdgeShader = Shader::Create("assets/shaders/PostProcess/EdgeExtract.glsl");
		m_CompositeShader = Shader::Create("assets/shaders/PostProcess/OutlineComposite.glsl");
		m_DebugShader = Shader::Create("assets/shaders/PostProcess/DebugVisualize.glsl");

		// ? NEW: Create UBOs for mask rendering (same bindings as main renderer)
		m_CameraUBO = UniformBuffer::Create(sizeof(glm::mat4), 0);
		m_TransformUBO = UniformBuffer::Create(sizeof(glm::mat4), 1);

		CreateFramebuffers();
		CreateFullscreenQuad();
	}

	void SelectionOutlinePass::Shutdown() {
		m_MaskFBO.reset();
		m_BlurFBO_A.reset();
		m_BlurFBO_B.reset();
		m_EdgeFBO.reset();
		m_FullscreenQuad.reset();
		m_FullscreenQuadVB.reset();
	}

	void SelectionOutlinePass::Execute(const PostProcessContext& ctx) {
		if (m_SelectedEntities.empty())
			return;

		RenderMaskPass(ctx);
		RenderBlurPass();
		RenderEdgePass();
		
		// ? DEBUG: Visualize intermediate stages or final composite
		if (m_Settings.DebugMode > 0) {
			RenderDebugVisualization(ctx);
		} else {
			RenderCompositePass(ctx);
		}
	}

	void SelectionOutlinePass::Resize(uint32_t width, uint32_t height) {
		if (width == m_Width && height == m_Height)
			return;

		m_Width = width;
		m_Height = height;

		CreateFramebuffers();
	}

	void SelectionOutlinePass::SetSelectedEntities(const std::set<Entity>& entities) {
		m_SelectedEntities.clear();
		m_SelectedEntities.reserve(entities.size());
		for (const auto& entity : entities) {
			m_SelectedEntities.push_back(entity);
		}
	}

	void SelectionOutlinePass::CreateFramebuffers() {
		// Mask FBO - R8 format for binary mask
		{
			FramebufferSpecification spec;
			spec.Width = m_Width;
			spec.Height = m_Height;
			spec.Attachments = {
				FramebufferTextureFormat::RGBA8,  // Need RGBA for compatibility
				FramebufferTextureFormat::Depth
			};
			spec.Samples = 1;
			m_MaskFBO = Framebuffer::Create(spec);
		}

		// Blur FBOs - R8 format for greyscale blur
		{
			FramebufferSpecification spec;
			spec.Width = m_Width;
			spec.Height = m_Height;
			spec.Attachments = {
				FramebufferTextureFormat::RGBA8
			};
			spec.Samples = 1;
			m_BlurFBO_A = Framebuffer::Create(spec);
			m_BlurFBO_B = Framebuffer::Create(spec);
		}

		// Edge FBO - R8 format for edge mask
		{
			FramebufferSpecification spec;
			spec.Width = m_Width;
			spec.Height = m_Height;
			spec.Attachments = {
				FramebufferTextureFormat::RGBA8
			};
			spec.Samples = 1;
			m_EdgeFBO = Framebuffer::Create(spec);
		}
	}

	void SelectionOutlinePass::CreateFullscreenQuad() {
		float quadVertices[] = {
			// positions   // texCoords
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,

			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f
		};

		m_FullscreenQuad = VertexArray::Create();
		m_FullscreenQuadVB = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
		m_FullscreenQuadVB->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		});
		m_FullscreenQuad->AddVertexBuffer(m_FullscreenQuadVB);
	}

	void SelectionOutlinePass::RenderMaskPass(const PostProcessContext& ctx) {
		// Bind mask framebuffer
		m_MaskFBO->Bind();
		
		// Clear to black (0 = no selection)
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Enable depth test
		glEnable(GL_DEPTH_TEST);

		// ? Set camera ViewProjection to UBO
		if (ctx.Camera) {
			glm::mat4 viewProjection = ctx.Camera->GetViewProjection();
			m_CameraUBO->SetData(&viewProjection, sizeof(glm::mat4));
		}

		// Bind mask shader
		m_MaskShader->Bind();

		// Render each selected entity
		for (auto& entity : m_SelectedEntities) {
			if (!entity)
				continue;

			auto& transform = entity.GetComponent<TransformComponent>();
			glm::mat4 transformMatrix = transform.GetTransform();

			// ? Set transform to UBO
			m_TransformUBO->SetData(&transformMatrix, sizeof(glm::mat4));

			// Check if entity has mesh component
			if (entity.HasComponent<MeshComponent>()) {
				auto& meshComp = entity.GetComponent<MeshComponent>();
				if (meshComp.MeshModel) {
					// Draw the model with mask shader
					meshComp.MeshModel->Draw(m_MaskShader);
				}
			}
		}

		m_MaskFBO->Unbind();
	}

	void SelectionOutlinePass::RenderBlurPass() {
		glDisable(GL_DEPTH_TEST);

		// Horizontal blur pass
		{
			m_BlurFBO_A->Bind();
			glClear(GL_COLOR_BUFFER_BIT);

			m_BlurShader->Bind();
			
			// ? Fix: Use correct method names (capital S)
			m_BlurShader->SetFloat2("u_Direction", glm::vec2(1.0f, 0.0f));
			m_BlurShader->SetFloat("u_BlurRadius", m_Settings.BlurRadius);
			m_BlurShader->SetFloat2("u_TexelSize", glm::vec2(1.0f / m_Width, 1.0f / m_Height));
			m_BlurShader->SetInt("u_Texture", 0);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_MaskFBO->GetColorAttachmentRendererID());

			m_FullscreenQuad->Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);

			m_BlurFBO_A->Unbind();
		}

		// Vertical blur pass
		{
			m_BlurFBO_B->Bind();
			glClear(GL_COLOR_BUFFER_BIT);

			m_BlurShader->Bind();
			m_BlurShader->SetFloat2("u_Direction", glm::vec2(0.0f, 1.0f));
			m_BlurShader->SetFloat("u_BlurRadius", m_Settings.BlurRadius);
			m_BlurShader->SetFloat2("u_TexelSize", glm::vec2(1.0f / m_Width, 1.0f / m_Height));
			m_BlurShader->SetInt("u_Texture", 0);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_BlurFBO_A->GetColorAttachmentRendererID());

			m_FullscreenQuad->Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);

			m_BlurFBO_B->Unbind();
		}

		glEnable(GL_DEPTH_TEST);
	}

	void SelectionOutlinePass::RenderEdgePass() {
		m_EdgeFBO->Bind();
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		m_EdgeShader->Bind();
		m_EdgeShader->SetInt("u_BlurredMask", 0);
		m_EdgeShader->SetInt("u_OriginalMask", 1);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_BlurFBO_B->GetColorAttachmentRendererID());
		
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_MaskFBO->GetColorAttachmentRendererID());

		m_FullscreenQuad->Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_EdgeFBO->Unbind();
		glEnable(GL_DEPTH_TEST);
	}

	void SelectionOutlinePass::RenderCompositePass(const PostProcessContext& ctx) {
		// ? FIX: We cannot read and write to the same FBO simultaneously
		// We need to blit the scene first, then draw outline on top
		
		// Disable depth test for post-processing
		glDisable(GL_DEPTH_TEST);
		
		// Render to output framebuffer (which should already have the scene rendered)
		ctx.OutputFBO->Bind();
		
		// Enable blending to composite outline ON TOP of existing scene
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		m_CompositeShader->Bind();
		
		// IMPORTANT: Since OutputFBO already contains the scene, we:
		// 1. Read the edge mask from m_EdgeFBO (texture unit 1)
		// 2. Draw a fullscreen quad with the outline color modulated by edge mask
		// 3. Blend it on top of the existing scene in OutputFBO
		
		m_CompositeShader->SetInt("u_EdgeMask", 0);  // Edge mask is all we need now
		m_CompositeShader->SetFloat4("u_OutlineColor", m_Settings.OutlineColor);
		m_CompositeShader->SetInt("u_BlendMode", m_Settings.BlendMode);
		m_CompositeShader->SetInt("u_DepthTest", m_Settings.DepthTest ? 1 : 0);

		// Bind edge mask texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_EdgeFBO->GetColorAttachmentRendererID());

		m_FullscreenQuad->Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		ctx.OutputFBO->Unbind();
	}

	void SelectionOutlinePass::RenderDebugVisualization(const PostProcessContext& ctx) {
		// ? DEBUG: Visualize intermediate stages
		ctx.OutputFBO->Bind();
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		m_DebugShader->Bind();
		m_DebugShader->SetInt("u_Texture", 0);

		glActiveTexture(GL_TEXTURE0);
		
		// Select which texture to visualize based on debug mode
		switch (m_Settings.DebugMode) {
			case 1: // Mask
				glBindTexture(GL_TEXTURE_2D, m_MaskFBO->GetColorAttachmentRendererID());
				break;
			case 2: // Blur
			 glBindTexture(GL_TEXTURE_2D, m_BlurFBO_B->GetColorAttachmentRendererID());
				break;
			case 3: // Edge
				glBindTexture(GL_TEXTURE_2D, m_EdgeFBO->GetColorAttachmentRendererID());
				break;
			default:
				break;
		}

		m_FullscreenQuad->Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glEnable(GL_DEPTH_TEST);
		ctx.OutputFBO->Unbind();
	}
}
