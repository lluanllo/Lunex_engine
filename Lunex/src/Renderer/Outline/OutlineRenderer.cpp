#include "stpch.h"
#include "OutlineRenderer.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Resources/Mesh/Model.h"
#include "Log/Log.h"
#include "RHI/RHI.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <vector>

namespace Lunex {

	OutlineRenderer& OutlineRenderer::Get() {
		static OutlineRenderer instance;
		return instance;
	}

	// ========================================================================
	// INITIALIZATION / SHUTDOWN
	// ========================================================================

	void OutlineRenderer::Initialize(uint32_t width, uint32_t height) {
		if (m_Initialized) return;
		if (width == 0 || height == 0) return;

		m_Width  = width;
		m_Height = height;

		CreateResources();
		CreateDebugMeshes();

		m_Initialized = true;
		LNX_LOG_INFO("OutlineRenderer initialized: {0}x{1}", m_Width, m_Height);
	}

	void OutlineRenderer::Shutdown() {
		if (!m_Initialized) return;

		m_SilhouetteFBO.reset();
		m_BlurPingFBO.reset();
		m_BlurPongFBO.reset();

		m_FullscreenQuadVA.reset();
		m_FullscreenQuadVB.reset();
		m_FullscreenQuadIB.reset();

		m_BoxVA.reset();
		m_BoxIB.reset();
		m_SphereVA.reset();
		m_SphereIB.reset();

		m_SilhouetteShader.reset();
		m_BlurShader.reset();
		m_CompositeShader.reset();
		m_SilhouetteUBO.reset();
		m_BlurUBO.reset();
		m_CompositeUBO.reset();

		m_Initialized = false;
		LNX_LOG_INFO("OutlineRenderer shut down");
	}

	void OutlineRenderer::OnViewportResize(uint32_t width, uint32_t height) {
		if (!m_Initialized) return;
		if (width == 0 || height == 0) return;
		if (width == m_Width && height == m_Height) return;

		m_Width  = width;
		m_Height = height;

		// Recreate FBOs at new size
		m_SilhouetteFBO.reset();
		m_BlurPingFBO.reset();
		m_BlurPongFBO.reset();

		// Silhouette FBO: RGBA8 color + depth
		{
			RHI::FramebufferDesc desc;
			desc.Width  = m_Width;
			desc.Height = m_Height;
			desc.AddColorAttachment(RHI::TextureFormat::RGBA8);
			desc.SetDepthAttachment(RHI::TextureFormat::Depth24Stencil8);
			desc.DebugName = "OutlineSilhouetteFBO";
			m_SilhouetteFBO = RHI::RHIFramebuffer::Create(desc);
		}

		// Blur ping FBO: RGBA8, no depth
		{
			RHI::FramebufferDesc desc;
			desc.Width  = m_Width;
			desc.Height = m_Height;
			desc.AddColorAttachment(RHI::TextureFormat::RGBA8);
			desc.DebugName = "OutlineBlurPingFBO";
			m_BlurPingFBO = RHI::RHIFramebuffer::Create(desc);
		}

		// Blur pong FBO: RGBA8, no depth
		{
			RHI::FramebufferDesc desc;
			desc.Width  = m_Width;
			desc.Height = m_Height;
			desc.AddColorAttachment(RHI::TextureFormat::RGBA8);
			desc.DebugName = "OutlineBlurPongFBO";
			m_BlurPongFBO = RHI::RHIFramebuffer::Create(desc);
		}

		LNX_LOG_INFO("OutlineRenderer resized: {0}x{1}", m_Width, m_Height);
	}

	// ========================================================================
	// CREATE RESOURCES
	// ========================================================================

	void OutlineRenderer::CreateResources() {
		// ---- Shaders ----
		m_SilhouetteShader = Shader::Create("assets/shaders/OutlineSilhouette.glsl");
		m_BlurShader       = Shader::Create("assets/shaders/OutlineBlur.glsl");
		m_CompositeShader  = Shader::Create("assets/shaders/OutlineComposite.glsl");

		// ---- UBOs ----
		m_SilhouetteUBO = UniformBuffer::Create(sizeof(SilhouetteUBOData), 8);
		memset(&m_SilhouetteUBOData, 0, sizeof(m_SilhouetteUBOData));

		m_BlurUBO = UniformBuffer::Create(sizeof(BlurUBOData), 9);
		memset(&m_BlurUBOData, 0, sizeof(m_BlurUBOData));

		m_CompositeUBO = UniformBuffer::Create(sizeof(CompositeUBOData), 10);
		memset(&m_CompositeUBOData, 0, sizeof(m_CompositeUBOData));

		// ---- Fullscreen quad ----
		float quadVertices[] = {
			// pos        // uv
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f,
			-1.0f,  1.0f,  0.0f, 1.0f,
		};
		uint32_t quadIndices[] = { 0, 1, 2, 2, 3, 0 };

		m_FullscreenQuadVA = VertexArray::Create();
		m_FullscreenQuadVB = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
		m_FullscreenQuadVB->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" },
		});
		m_FullscreenQuadVA->AddVertexBuffer(m_FullscreenQuadVB);
		m_FullscreenQuadIB = IndexBuffer::Create(quadIndices, 6);
		m_FullscreenQuadVA->SetIndexBuffer(m_FullscreenQuadIB);

		// ---- FBOs ----
		// Silhouette FBO
		{
			RHI::FramebufferDesc desc;
			desc.Width  = m_Width;
			desc.Height = m_Height;
			desc.AddColorAttachment(RHI::TextureFormat::RGBA8);
			desc.SetDepthAttachment(RHI::TextureFormat::Depth24Stencil8);
			desc.DebugName = "OutlineSilhouetteFBO";
			m_SilhouetteFBO = RHI::RHIFramebuffer::Create(desc);
		}

		// Blur ping
		{
			RHI::FramebufferDesc desc;
			desc.Width  = m_Width;
			desc.Height = m_Height;
			desc.AddColorAttachment(RHI::TextureFormat::RGBA8);
			desc.DebugName = "OutlineBlurPingFBO";
			m_BlurPingFBO = RHI::RHIFramebuffer::Create(desc);
		}

		// Blur pong
		{
			RHI::FramebufferDesc desc;
			desc.Width  = m_Width;
			desc.Height = m_Height;
			desc.AddColorAttachment(RHI::TextureFormat::RGBA8);
			desc.DebugName = "OutlineBlurPongFBO";
			m_BlurPongFBO = RHI::RHIFramebuffer::Create(desc);
		}
	}

	void OutlineRenderer::CreateDebugMeshes() {
		// ---- Unit Cube (for BoxCollider3D) ----
		{
			float v[] = {
				-0.5f,-0.5f,-0.5f,   0.5f,-0.5f,-0.5f,   0.5f, 0.5f,-0.5f,  -0.5f, 0.5f,-0.5f,
				-0.5f,-0.5f, 0.5f,   0.5f,-0.5f, 0.5f,   0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,
			};
			uint32_t idx[] = {
				0,1,2, 2,3,0,   // back
				4,5,6, 6,7,4,   // front
				0,4,7, 7,3,0,   // left
				1,5,6, 6,2,1,   // right
				3,2,6, 6,7,3,   // top
				0,1,5, 5,4,0,   // bottom
			};

			m_BoxVA = VertexArray::Create();
			auto vb = VertexBuffer::Create(v, sizeof(v));
			vb->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
			m_BoxVA->AddVertexBuffer(vb);
			m_BoxIB = IndexBuffer::Create(idx, 36);
			m_BoxVA->SetIndexBuffer(m_BoxIB);
			m_BoxIndexCount = 36;
		}

		// ---- Unit Sphere (for SphereCollider3D) ----
		{
			const int stacks = 12;
			const int slices = 16;
			std::vector<float> verts;
			std::vector<uint32_t> inds;

			for (int i = 0; i <= stacks; ++i) {
				float phi = glm::pi<float>() * static_cast<float>(i) / stacks;
				for (int j = 0; j <= slices; ++j) {
					float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / slices;
					float x = std::sin(phi) * std::cos(theta);
					float y = std::cos(phi);
					float z = std::sin(phi) * std::sin(theta);
					verts.push_back(x * 0.5f);
					verts.push_back(y * 0.5f);
					verts.push_back(z * 0.5f);
				}
			}
			for (int i = 0; i < stacks; ++i) {
				for (int j = 0; j < slices; ++j) {
					uint32_t a = i * (slices + 1) + j;
					uint32_t b = a + slices + 1;
					inds.push_back(a); inds.push_back(b); inds.push_back(a + 1);
					inds.push_back(a + 1); inds.push_back(b); inds.push_back(b + 1);
				}
			}

			m_SphereVA = VertexArray::Create();
			auto vb = VertexBuffer::Create(verts.data(), static_cast<uint32_t>(verts.size() * sizeof(float)));
			vb->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
			m_SphereVA->AddVertexBuffer(vb);
			m_SphereIB = IndexBuffer::Create(inds.data(), static_cast<uint32_t>(inds.size()));
			m_SphereVA->SetIndexBuffer(m_SphereIB);
			m_SphereIndexCount = static_cast<uint32_t>(inds.size());
		}
	}

	// ========================================================================
	// FULLSCREEN QUAD HELPER
	// ========================================================================

	void OutlineRenderer::DrawFullscreenQuad() {
		auto* cmd = RHI::GetImmediateCommandList();
		m_FullscreenQuadVA->Bind();
		cmd->DrawIndexed(6);
	}

	// ========================================================================
	// SILHOUETTE PASS
	// ========================================================================

	void OutlineRenderer::BeginSilhouettePass() {
		auto* cmd = RHI::GetImmediateCommandList();

		m_SilhouetteFBO->Bind();
		cmd->SetViewport(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));

		// Clear to black (no silhouette)
		cmd->SetClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
		cmd->Clear();

		// Enable depth test so occluded parts are not drawn (optional)
		cmd->SetDepthTestEnabled(true);
		cmd->SetDepthMask(true);
		cmd->SetDepthFunc(RHI::CompareFunc::Less);
		cmd->SetCullMode(RHI::CullMode::None);

		m_SilhouetteShader->Bind();
		m_SilhouetteHasContent = false;
	}

	void OutlineRenderer::EndSilhouettePass() {
		m_SilhouetteFBO->Unbind();
	}

	void OutlineRenderer::DrawEntitySilhouette(Scene* scene, Entity entity, const glm::mat4& viewProjection, const glm::vec4& color) {
		if (!entity) return;
		auto* cmd = RHI::GetImmediateCommandList();

		glm::mat4 worldTransform = scene->GetWorldTransform(entity);

		m_SilhouetteUBOData.ViewProjection = viewProjection;
		m_SilhouetteUBOData.Color = color;

		// 3D mesh entities
		if (entity.HasComponent<MeshComponent>()) {
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (mesh.MeshModel) {
				m_SilhouetteUBOData.Model = worldTransform;
				m_SilhouetteUBO->SetData(&m_SilhouetteUBOData, sizeof(SilhouetteUBOData));

				for (const auto& submesh : mesh.MeshModel->GetMeshes()) {
					auto va = submesh->GetVertexArray();
					if (va) {
						va->Bind();
						cmd->DrawIndexed(static_cast<uint32_t>(submesh->GetIndices().size()));
						m_SilhouetteHasContent = true;
					}
				}
			}
		}

		// 2D sprite entities — draw a unit quad at the entity transform
		if (entity.HasComponent<SpriteRendererComponent>()) {
			m_SilhouetteUBOData.Model = worldTransform;
			m_SilhouetteUBO->SetData(&m_SilhouetteUBOData, sizeof(SilhouetteUBOData));

			if (m_BoxVA) {
				// Use the box mesh scaled flat as a quad
				m_BoxVA->Bind();
				cmd->DrawIndexed(m_BoxIndexCount);
				m_SilhouetteHasContent = true;
			}
		}

		// Circle renderer entities
		if (entity.HasComponent<CircleRendererComponent>()) {
			m_SilhouetteUBOData.Model = worldTransform;
			m_SilhouetteUBO->SetData(&m_SilhouetteUBOData, sizeof(SilhouetteUBOData));

			if (m_SphereVA) {
				m_SphereVA->Bind();
				cmd->DrawIndexed(m_SphereIndexCount);
				m_SilhouetteHasContent = true;
			}
		}
	}

	void OutlineRenderer::DrawColliderSilhouette(const glm::mat4& viewProjection, const glm::mat4& model, const glm::vec4& color) {
		// Common setup — caller decides which VA to bind
		m_SilhouetteUBOData.ViewProjection = viewProjection;
		m_SilhouetteUBOData.Model = model;
		m_SilhouetteUBOData.Color = color;
		m_SilhouetteUBO->SetData(&m_SilhouetteUBOData, sizeof(SilhouetteUBOData));
		m_SilhouetteHasContent = true;
	}

	// ========================================================================
	// RENDER SELECTION OUTLINE (complete cycle: silhouette ? blur ? composite)
	// ========================================================================

	void OutlineRenderer::RenderSelectionOutline(
		Scene* scene,
		const std::set<Entity>& selectedEntities,
		const glm::mat4& viewProjection,
		uint64_t targetFBOHandle,
		const glm::vec4& outlineColor)
	{
		if (!m_Initialized || !scene || selectedEntities.empty()) return;

		// Save state
		auto* cmd = RHI::GetImmediateCommandList();
		int prevViewport[4];
		cmd->GetViewport(prevViewport);
		uint64_t prevFBO = cmd->GetBoundFramebuffer();

		// 1. Silhouette pass
		BeginSilhouettePass();

		glm::vec4 white(1.0f);
		for (const auto& entity : selectedEntities) {
			Entity e = entity;
			DrawEntitySilhouette(scene, e, viewProjection, white);
		}

		EndSilhouettePass();

		// 2. Blur + Composite (only if something was drawn)
		if (m_SilhouetteHasContent) {
			BlurPass();
			CompositePass(targetFBOHandle, outlineColor);
		}

		m_SilhouetteHasContent = false;

		// Restore previous FBO and viewport
		cmd->BindFramebufferByHandle(prevFBO);
		cmd->SetViewport(static_cast<float>(prevViewport[0]), static_cast<float>(prevViewport[1]),
			static_cast<float>(prevViewport[2]), static_cast<float>(prevViewport[3]));
	}

	// ========================================================================
	// RENDER COLLIDER OUTLINES (complete cycle: silhouette ? blur ? composite)
	// ========================================================================

	void OutlineRenderer::RenderColliderOutlines(
		Scene* scene,
		const glm::mat4& viewProjection,
		uint64_t targetFBOHandle,
		bool show3D,
		bool show2D,
		const glm::vec4& collider3DColor,
		const glm::vec4& collider2DColor)
	{
		if (!m_Initialized || !scene) return;
		if (!show3D && !show2D) return;

		auto* cmd = RHI::GetImmediateCommandList();
		int prevViewport[4];
		cmd->GetViewport(prevViewport);
		uint64_t prevFBO = cmd->GetBoundFramebuffer();

		// We use one common color for all colliders in a single pass.
		// If both 3D and 2D are shown, we merge them into one outline pass with a blended color.
		// For distinct colors per type, we do separate passes.

		auto renderColliderPass = [&](const glm::vec4& outlineColor, auto renderFunc) {
			BeginSilhouettePass();
			renderFunc();
			EndSilhouettePass();

			if (m_SilhouetteHasContent) {
				BlurPass();
				CompositePass(targetFBOHandle, outlineColor);
			}
			m_SilhouetteHasContent = false;
		};

		// 3D colliders pass (green)
		if (show3D) {
			renderColliderPass(collider3DColor, [&]() {
				glm::vec4 white(1.0f);

				// Box Colliders
				auto boxView = scene->GetAllEntitiesWith<TransformComponent, BoxCollider3DComponent>();
				for (auto entityID : boxView) {
					auto [tc, bc3d] = boxView.get<TransformComponent, BoxCollider3DComponent>(entityID);
					glm::vec3 translation = tc.Translation + bc3d.Offset;
					glm::vec3 scale = tc.Scale * (bc3d.HalfExtents * 2.0f);
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::toMat4(glm::quat(tc.Rotation))
						* glm::scale(glm::mat4(1.0f), scale);

					DrawColliderSilhouette(viewProjection, transform, white);
					m_BoxVA->Bind();
					cmd->DrawIndexed(m_BoxIndexCount);
				}

				// Sphere Colliders
				auto sphereView = scene->GetAllEntitiesWith<TransformComponent, SphereCollider3DComponent>();
				for (auto entityID : sphereView) {
					auto [tc, sc3d] = sphereView.get<TransformComponent, SphereCollider3DComponent>(entityID);
					glm::vec3 translation = tc.Translation + sc3d.Offset;
					glm::vec3 scale = tc.Scale * glm::vec3(sc3d.Radius * 2.0f);
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::scale(glm::mat4(1.0f), scale);

					DrawColliderSilhouette(viewProjection, transform, white);
					m_SphereVA->Bind();
					cmd->DrawIndexed(m_SphereIndexCount);
				}

				// Capsule Colliders (approximate as scaled sphere)
				auto capsuleView = scene->GetAllEntitiesWith<TransformComponent, CapsuleCollider3DComponent>();
				for (auto entityID : capsuleView) {
					auto [tc, cc3d] = capsuleView.get<TransformComponent, CapsuleCollider3DComponent>(entityID);
					glm::vec3 translation = tc.Translation + cc3d.Offset;
					glm::vec3 scale = tc.Scale * glm::vec3(cc3d.Radius * 2.0f, cc3d.Height, cc3d.Radius * 2.0f);
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::toMat4(glm::quat(tc.Rotation))
						* glm::scale(glm::mat4(1.0f), scale);

					DrawColliderSilhouette(viewProjection, transform, white);
					m_SphereVA->Bind();
					cmd->DrawIndexed(m_SphereIndexCount);
				}
			});
		}

		// 2D colliders pass (red)
		if (show2D) {
			renderColliderPass(collider2DColor, [&]() {
				glm::vec4 white(1.0f);

				// Box 2D Colliders
				auto box2DView = scene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
				for (auto entityID : box2DView) {
					auto [tc, bc2d] = box2DView.get<TransformComponent, BoxCollider2DComponent>(entityID);
					glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0, 0, 1))
						* glm::scale(glm::mat4(1.0f), scale);

					DrawColliderSilhouette(viewProjection, transform, white);
					m_BoxVA->Bind();
					cmd->DrawIndexed(m_BoxIndexCount);
				}

				// Circle 2D Colliders
				auto circle2DView = scene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
				for (auto entityID : circle2DView) {
					auto [tc, cc2d] = circle2DView.get<TransformComponent, CircleCollider2DComponent>(entityID);
					glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::scale(glm::mat4(1.0f), scale);

					DrawColliderSilhouette(viewProjection, transform, white);
					m_SphereVA->Bind();
					cmd->DrawIndexed(m_SphereIndexCount);
				}
			});
		}

		// Restore
		cmd->BindFramebufferByHandle(prevFBO);
		cmd->SetViewport(static_cast<float>(prevViewport[0]), static_cast<float>(prevViewport[1]),
			static_cast<float>(prevViewport[2]), static_cast<float>(prevViewport[3]));
	}

	// ========================================================================
	// BLUR PASS (two-pass separable)
	// ========================================================================

	void OutlineRenderer::BlurPass() {
		auto* cmd = RHI::GetImmediateCommandList();

		cmd->SetDepthTestEnabled(false);
		cmd->SetDepthMask(false);
		cmd->SetCullMode(RHI::CullMode::None);

		m_BlurShader->Bind();

		float texelW = 1.0f / static_cast<float>(m_Width);
		float texelH = 1.0f / static_cast<float>(m_Height);

		// ---- Horizontal blur: silhouette ? ping ----
		m_BlurPingFBO->Bind();
		cmd->SetViewport(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));
		cmd->SetClearColor(glm::vec4(0.0f));
		cmd->Clear();

		auto silhouetteColor = m_SilhouetteFBO->GetColorAttachment(0);
		if (silhouetteColor) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(silhouetteColor->GetNativeHandle()));
		}

		m_BlurUBOData.TexelSize = glm::vec2(texelW, texelH);
		m_BlurUBOData.Direction = glm::vec2(1.0f, 0.0f);
		m_BlurUBOData.KernelSize = m_Config.KernelSize;
		m_BlurUBO->SetData(&m_BlurUBOData, sizeof(BlurUBOData));

		DrawFullscreenQuad();
		m_BlurPingFBO->Unbind();

		// ---- Vertical blur: ping ? pong ----
		m_BlurPongFBO->Bind();
		cmd->SetViewport(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));
		cmd->SetClearColor(glm::vec4(0.0f));
		cmd->Clear();

		auto pingColor = m_BlurPingFBO->GetColorAttachment(0);
		if (pingColor) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(pingColor->GetNativeHandle()));
		}

		m_BlurUBOData.Direction = glm::vec2(0.0f, 1.0f);
		m_BlurUBO->SetData(&m_BlurUBOData, sizeof(BlurUBOData));

		DrawFullscreenQuad();
		m_BlurPongFBO->Unbind();
	}

	// ========================================================================
	// COMPOSITE PASS
	// ========================================================================

	void OutlineRenderer::CompositePass(uint64_t targetFBOHandle, const glm::vec4& outlineColor) {
		auto* cmd = RHI::GetImmediateCommandList();

		cmd->BindFramebufferByHandle(targetFBOHandle);
		cmd->SetViewport(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));

		cmd->SetDepthTestEnabled(false);
		cmd->SetDepthMask(false);
		cmd->SetCullMode(RHI::CullMode::None);

		// Enable alpha blending
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		m_CompositeShader->Bind();

		// Bind blurred silhouette (pong)
		auto pongColor = m_BlurPongFBO->GetColorAttachment(0);
		if (pongColor) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(pongColor->GetNativeHandle()));
		}

		// Bind original silhouette
		auto silhouetteColor = m_SilhouetteFBO->GetColorAttachment(0);
		if (silhouetteColor) {
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(silhouetteColor->GetNativeHandle()));
		}

		m_CompositeUBOData.OutlineColor = outlineColor;
		m_CompositeUBOData.OutlineHardness = m_Config.OutlineHardness;
		m_CompositeUBOData.InsideAlpha = m_Config.InsideAlpha;
		m_CompositeUBO->SetData(&m_CompositeUBOData, sizeof(CompositeUBOData));

		DrawFullscreenQuad();

		// Restore blend state
		glDisable(GL_BLEND);

		// Restore depth state
		cmd->SetDepthTestEnabled(true);
		cmd->SetDepthMask(true);
	}

	// ========================================================================
	// COMPOSITE (PUBLIC) - No-op, each pass now composites internally
	// ========================================================================

	void OutlineRenderer::Composite(uint64_t targetFBOHandle) {
		// Each RenderXxxOutline call now performs its own complete cycle.
		// This method is kept for API compatibility but does nothing.
	}

} // namespace Lunex
