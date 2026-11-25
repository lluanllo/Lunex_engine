#include "stpch.h"
#include "DeferredRenderer.h"

#include "RHI/RHI.h"
#include "RHI/OpenGL/OpenGLRHIFramebuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/SkinningBuffer.h"
#include "Renderer/Shadows/ShadowSystem.h"
#include "Renderer/PostProcess/PostProcessRenderer.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Resources/Mesh/Model.h"
#include "Scene/Components.h"
#include "Scene/Lighting/LightTypes.h"
#include "Scene/Camera/CameraData.h"
#include "Log/Log.h"

#include <glm/gtc/type_ptr.hpp>

namespace Lunex {

	// ============================================================================
	// INTERNAL DATA
	// ============================================================================

	struct DeferredRendererData {
		// ========== STATE ==========
		bool Enabled = true;
		bool Initialized = false;

		// ========== G-BUFFER ==========
		GBuffer GBuffer;

		// ========== SHADERS ==========
		Ref<Shader> GeometryShader;    // Geometry pass: writes G-Buffer MRT
		Ref<Shader> LightingShader;    // Lighting pass: full-screen quad, reads G-Buffer

		// ========== FULL-SCREEN QUAD ==========
		Ref<VertexArray> QuadVAO;
		Ref<VertexBuffer> QuadVBO;

		// ========== UNIFORM BUFFERS ==========
		// Camera UBO (binding 0) - shared with forward renderer
		struct CameraData {
			glm::mat4 ViewProjection;
			glm::mat4 View;
			glm::mat4 Projection;
			glm::vec3 ViewPos;
			float _padding;
		};

		// Matches std140 layout of 'Transform' UBO (binding 1) in Mesh3D / Deferred_Geometry
		struct TransformData {
			glm::mat4 Transform;
			glm::mat4 NormalMatrix;
			int EntityID;
			int Skinned;
			int DebugBone = -1;
			int _pad = 0;
		};

		// Post-process control UBO (binding 6) - matches shader layout
		struct PostProcessControlData {
			int SkipToneMapGamma;
			float _pad1;
			float _pad2;
			float _pad3;
		};

		// Material UBO uses same layout as forward renderer (binding 2)
		struct MaterialUniformData {
			glm::vec4 Color;
			float Metallic;
			float Roughness;
			float Specular;
			float EmissionIntensity;
			glm::vec3 EmissionColor;
			float NormalIntensity;
			glm::vec3 ViewPos;

			int UseAlbedoMap;
			int UseNormalMap;
			int UseMetallicMap;
			int UseRoughnessMap;
			int UseSpecularMap;
			int UseEmissionMap;
			int UseAOMap;
			float _padding2;

			float MetallicMultiplier;
			float RoughnessMultiplier;
			float SpecularMultiplier;
			float AOMultiplier;

			int DetailNormalCount;
			int UseLayeredTexture;
			int LayeredMetallicChannel;
			int LayeredRoughnessChannel;

			int LayeredAOChannel;
			int LayeredUseMetallic;
			int LayeredUseRoughness;
			int LayeredUseAO;

			float _detailPad0;

			glm::vec4 DetailNormalIntensities;
			glm::vec4 DetailNormalTilingX;
			glm::vec4 DetailNormalTilingY;

			glm::vec4 SubsurfaceColorAmount;   // rgb color, a amount
			glm::vec4 UVTilingOffset = { 1.0f, 1.0f, 0.0f, 0.0f };
		};

		CameraData CameraBuffer;
		TransformData TransformBuffer;
		MaterialUniformData MaterialBuffer;
		PostProcessControlData PostProcessControlBuffer;

		Ref<UniformBuffer> CameraUniformBuffer;
		Ref<UniformBuffer> TransformUniformBuffer;
		Ref<UniformBuffer> MaterialUniformBuffer;
		Ref<UniformBuffer> PostProcessControlUniformBuffer;

		// ========== CAMERA STATE ==========
		glm::vec3 CameraPosition = { 0.0f, 0.0f, 0.0f };
		glm::mat4 ViewMatrix = glm::mat4(1.0f);
		glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
		ViewFrustum CameraFrustum;
		bool FrustumCullingEnabled = true;

		// ========== STATISTICS ==========
		DeferredRenderer::Statistics Stats;
	};

	static DeferredRendererData s_Data;

	// ============================================================================
	// INIT / SHUTDOWN
	// ============================================================================

	void DeferredRenderer::Init() {
		LNX_PROFILE_FUNCTION();

		// Load shaders
		s_Data.GeometryShader = Shader::Create("assets/shaders/Deferred_Geometry.glsl");
		s_Data.LightingShader = Shader::Create("assets/shaders/Deferred_Lighting.glsl");

		if (!s_Data.GeometryShader || !s_Data.GeometryShader->IsValid()) {
			LNX_LOG_ERROR("DeferredRenderer: Failed to compile Deferred_Geometry shader!");
		}
		if (!s_Data.LightingShader || !s_Data.LightingShader->IsValid()) {
			LNX_LOG_ERROR("DeferredRenderer: Failed to compile Deferred_Lighting shader!");
		}

		// Create UBOs (same bindings as forward renderer for compatibility)
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(DeferredRendererData::CameraData), 0);
		s_Data.TransformUniformBuffer = UniformBuffer::Create(sizeof(DeferredRendererData::TransformData), 1);
		s_Data.MaterialUniformBuffer = UniformBuffer::Create(sizeof(DeferredRendererData::MaterialUniformData), 2);
		s_Data.PostProcessControlUniformBuffer = UniformBuffer::Create(sizeof(DeferredRendererData::PostProcessControlData), 6);

		// Create full-screen quad for lighting pass (6 vertices, 2 triangles)
		float quadVertices[] = {
			// positions        // texcoords
			-1.0f,  1.0f, 0.0f,  0.0f, 1.0f,   // TL
			-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,   // BL
			 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,   // BR

			-1.0f,  1.0f, 0.0f,  0.0f, 1.0f,   // TL
			 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,   // BR
			 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,   // TR
		};

		s_Data.QuadVAO = VertexArray::Create();
		s_Data.QuadVBO = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
		s_Data.QuadVBO->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoords" }
		});
		s_Data.QuadVAO->AddVertexBuffer(s_Data.QuadVBO);

		// Initialize Post-Processing
		PostProcessRenderer::Init();

		s_Data.Initialized = true;
		LNX_LOG_INFO("DeferredRenderer initialized");
	}

	void DeferredRenderer::Shutdown() {
		LNX_PROFILE_FUNCTION();
		PostProcessRenderer::Shutdown();
		s_Data.Initialized = false;
		LNX_LOG_INFO("DeferredRenderer shutdown");
	}

	bool DeferredRenderer::IsEnabled() {
		return s_Data.Enabled && s_Data.Initialized;
	}

	void DeferredRenderer::SetEnabled(bool enabled) {
		s_Data.Enabled = enabled;
	}

	// ============================================================================
	// SCENE MANAGEMENT
	// ============================================================================

	void DeferredRenderer::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();

		s_Data.CameraPosition = camera.GetPosition();
		s_Data.ViewMatrix = camera.GetViewMatrix();
		s_Data.ProjectionMatrix = camera.GetProjection();

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraBuffer.View = s_Data.ViewMatrix;
		s_Data.CameraBuffer.Projection = s_Data.ProjectionMatrix;
		s_Data.CameraBuffer.ViewPos = s_Data.CameraPosition;
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(DeferredRendererData::CameraData));
		s_Data.CameraFrustum = ViewFrustum::FromViewProjection(s_Data.CameraBuffer.ViewProjection);

		// Bind shadow atlas for reading
		ShadowSystem::Get().BindForSceneRendering();
	}

	void DeferredRenderer::BeginScene(const Camera& camera, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();

		s_Data.CameraPosition = glm::vec3(transform[3]);
		s_Data.ViewMatrix = glm::inverse(transform);
		s_Data.ProjectionMatrix = camera.GetProjection();

		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * s_Data.ViewMatrix;
		s_Data.CameraBuffer.View = s_Data.ViewMatrix;
		s_Data.CameraBuffer.Projection = s_Data.ProjectionMatrix;
		s_Data.CameraBuffer.ViewPos = s_Data.CameraPosition;
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(DeferredRendererData::CameraData));
		s_Data.CameraFrustum = ViewFrustum::FromViewProjection(s_Data.CameraBuffer.ViewProjection);

		ShadowSystem::Get().BindForSceneRendering();
	}

	void DeferredRenderer::EndScene() {
		LNX_PROFILE_FUNCTION();
	}

	// ============================================================================
	// GEOMETRY PASS
	// ============================================================================

	// Draws every submesh of a model whose world AABB intersects the camera frustum.
	static void DrawModelCulled(Model& model, const glm::mat4& transform, const Ref<Shader>& shader) {
		for (const auto& mesh : model.GetMeshes()) {
			// Skinned meshes deform beyond their bind-pose bounds: never cull them
			if (s_Data.FrustumCullingEnabled && !mesh->IsSkinned()) {
				glm::vec3 wMin, wMax;
				Mesh::TransformAABB(transform, mesh->GetAABBMin(), mesh->GetAABBMax(), wMin, wMax);
				if (!s_Data.CameraFrustum.IntersectsAABB(wMin, wMax)) {
					s_Data.Stats.CulledMeshes++;
					continue;
				}
			}
			mesh->Draw(shader);
			s_Data.Stats.GeometryDrawCalls++;
			s_Data.Stats.MeshCount++;
			s_Data.Stats.TriangleCount += static_cast<uint32_t>(mesh->GetIndices().size()) / 3;
		}
	}

	void DeferredRenderer::SetFrustumCullingEnabled(bool enabled) { s_Data.FrustumCullingEnabled = enabled; }
	bool DeferredRenderer::IsFrustumCullingEnabled() { return s_Data.FrustumCullingEnabled; }

	void DeferredRenderer::BeginGeometryPass() {
		LNX_PROFILE_FUNCTION();

		if (!s_Data.GBuffer.IsInitialized()) {
			return;
		}

		s_Data.GBuffer.Bind();

		// G-Buffer targets store data in alpha (metallic, roughness, AO, specular): blending
		// must be off, whatever the previous pass (2D overlays, UI) left enabled.
		glDisable(GL_BLEND);

		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetViewport(0.0f, 0.0f, 
				static_cast<float>(s_Data.GBuffer.GetWidth()), 
				static_cast<float>(s_Data.GBuffer.GetHeight()));
			cmdList->SetClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
			cmdList->Clear();
		}

		s_Data.GBuffer.ClearEntityID();
	}

	void DeferredRenderer::EndGeometryPass() {
		LNX_PROFILE_FUNCTION();
		s_Data.GBuffer.Unbind();
	}

	template<typename TUniformData>
	static void FillMaterialBuffer(const TUniformData& u) {
		auto& m = s_Data.MaterialBuffer;
		m.Color = u.Albedo;
		m.Metallic = u.Metallic;
		m.Roughness = u.Roughness;
		m.Specular = u.Specular;
		m.EmissionIntensity = u.EmissionIntensity;
		m.EmissionColor = u.EmissionColor;
		m.NormalIntensity = u.NormalIntensity;
		m.ViewPos = s_Data.CameraPosition;

		m.UseAlbedoMap = u.UseAlbedoMap;
		m.UseNormalMap = u.UseNormalMap;
		m.UseMetallicMap = u.UseMetallicMap;
		m.UseRoughnessMap = u.UseRoughnessMap;
		m.UseSpecularMap = u.UseSpecularMap;
		m.UseEmissionMap = u.UseEmissionMap;
		m.UseAOMap = u.UseAOMap;

		m.MetallicMultiplier = u.MetallicMultiplier;
		m.RoughnessMultiplier = u.RoughnessMultiplier;
		m.SpecularMultiplier = u.SpecularMultiplier;
		m.AOMultiplier = u.AOMultiplier;

		m.DetailNormalCount = u.DetailNormalCount;
		m.UseLayeredTexture = u.UseLayeredTexture;
		m.LayeredMetallicChannel = u.LayeredMetallicChannel;
		m.LayeredRoughnessChannel = u.LayeredRoughnessChannel;
		m.LayeredAOChannel = u.LayeredAOChannel;
		m.LayeredUseMetallic = u.LayeredUseMetallic;
		m.LayeredUseRoughness = u.LayeredUseRoughness;
		m.LayeredUseAO = u.LayeredUseAO;
		m.DetailNormalIntensities = u.DetailNormalIntensities;
		m.DetailNormalTilingX = u.DetailNormalTilingX;
		m.DetailNormalTilingY = u.DetailNormalTilingY;
		m.SubsurfaceColorAmount = u.SubsurfaceColorAmount;
		m.UVTilingOffset = u.UVTilingOffset;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(DeferredRendererData::MaterialUniformData));
	}

	static void UploadTransform(const glm::mat4& transform, const Model& model, int entityID, const std::vector<glm::mat4>* bones) {
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformBuffer.NormalMatrix = glm::transpose(glm::inverse(transform));
		s_Data.TransformBuffer.EntityID = entityID;
		s_Data.TransformBuffer.Skinned = model.IsSkinned() ? SkinningBuffer::Upload(bones) : 0;
		s_Data.TransformBuffer.DebugBone = s_Data.TransformBuffer.Skinned ? SkinningBuffer::GetDebugBone() : -1;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(DeferredRendererData::TransformData));
	}

	static void BindMaterialAssetTextures(const Ref<MaterialAsset>& mat) {
		if (mat->HasAlbedoMap()) mat->GetAlbedoMap()->Bind(0); else Texture2D::UnbindSlot(0);
		if (mat->HasNormalMap()) mat->GetNormalMap()->Bind(1); else Texture2D::UnbindSlot(1);
		if (mat->HasMetallicMap()) mat->GetMetallicMap()->Bind(2); else Texture2D::UnbindSlot(2);
		if (mat->HasRoughnessMap()) mat->GetRoughnessMap()->Bind(3); else Texture2D::UnbindSlot(3);
		if (mat->HasSpecularMap()) mat->GetSpecularMap()->Bind(4); else Texture2D::UnbindSlot(4);
		if (mat->HasEmissionMap()) mat->GetEmissionMap()->Bind(5); else Texture2D::UnbindSlot(5);
		if (mat->HasAOMap()) mat->GetAOMap()->Bind(6); else Texture2D::UnbindSlot(6);
		if (mat->HasLayeredTexture()) mat->GetLayeredTextureConfig().Texture->Bind(7); else Texture2D::UnbindSlot(7);
	}

	static bool IsSubmeshCulled(const Ref<Mesh>& mesh, const glm::mat4& transform) {
		// Skinned meshes deform beyond their bind-pose bounds: never cull them
		if (!s_Data.FrustumCullingEnabled || mesh->IsSkinned()) return false;
		glm::vec3 wMin, wMax;
		Mesh::TransformAABB(transform, mesh->GetAABBMin(), mesh->GetAABBMax(), wMin, wMax);
		if (s_Data.CameraFrustum.IntersectsAABB(wMin, wMax)) return false;
		s_Data.Stats.CulledMeshes++;
		return true;
	}

	// Imported models: one material per submesh. `fallback` binds the entity material when a
	// submesh has none (may be null -> default material).
	static void DrawPerSubmeshMaterials(const glm::mat4& transform, MeshComponent& meshComponent, MaterialInstance* fallback) {
		const auto& meshes = meshComponent.MeshModel->GetMeshes();
		for (size_t i = 0; i < meshes.size(); i++) {
			if (IsSubmeshCulled(meshes[i], transform)) continue;

			Ref<MaterialAsset> submeshMaterial = meshComponent.GetSubmeshMaterial(static_cast<uint32_t>(i));
			if (submeshMaterial) {
				FillMaterialBuffer(submeshMaterial->GetUniformData());
				s_Data.GeometryShader->Bind();
				BindMaterialAssetTextures(submeshMaterial);
			}
			else if (fallback) {
				FillMaterialBuffer(fallback->GetUniformData());
				s_Data.GeometryShader->Bind();
				fallback->BindTextures();
			}
			else {
				s_Data.GeometryShader->Bind();
				for (int slot = 0; slot < 8; slot++) Texture2D::UnbindSlot(slot);
			}

			meshes[i]->Draw(s_Data.GeometryShader);
			s_Data.Stats.GeometryDrawCalls++;
			s_Data.Stats.MeshCount++;
			s_Data.Stats.TriangleCount += static_cast<uint32_t>(meshes[i]->GetIndices().size()) / 3;
		}
	}

	void DeferredRenderer::SubmitMesh(const glm::mat4& transform, MeshComponent& meshComponent,
									  MaterialComponent& materialComponent, int entityID, const std::vector<glm::mat4>* bones) {
		if (!meshComponent.MeshModel || !materialComponent.Instance)
			return;

		if (!s_Data.GeometryShader || !s_Data.GeometryShader->IsValid() || !s_Data.GBuffer.IsInitialized())
			return;

		UploadTransform(transform, *meshComponent.MeshModel, entityID, bones);

		if (meshComponent.HasPerSubmeshMaterials()) {
			DrawPerSubmeshMaterials(transform, meshComponent, materialComponent.Instance.get());
			return;
		}

		FillMaterialBuffer(materialComponent.Instance->GetUniformData());
		s_Data.GeometryShader->Bind();
		materialComponent.Instance->BindTextures();
		DrawModelCulled(*meshComponent.MeshModel, transform, s_Data.GeometryShader);
	}

	void DeferredRenderer::SubmitMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID, const std::vector<glm::mat4>* bones) {
		if (!meshComponent.MeshModel)
			return;

		if (!s_Data.GeometryShader || !s_Data.GeometryShader->IsValid() || !s_Data.GBuffer.IsInitialized())
			return;

		UploadTransform(transform, *meshComponent.MeshModel, entityID, bones);

		// Default material (also the fallback for submeshes without an imported material)
		memset(&s_Data.MaterialBuffer, 0, sizeof(DeferredRendererData::MaterialUniformData));
		s_Data.MaterialBuffer.Color = glm::vec4(1.0f);
		s_Data.MaterialBuffer.Roughness = 0.5f;
		s_Data.MaterialBuffer.Specular = 0.5f;
		s_Data.MaterialBuffer.NormalIntensity = 1.0f;
		s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
		s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
		s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
		s_Data.MaterialBuffer.AOMultiplier = 1.0f;
		s_Data.MaterialBuffer.UVTilingOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
		s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;
		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(DeferredRendererData::MaterialUniformData));

		if (meshComponent.HasPerSubmeshMaterials()) {
			DrawPerSubmeshMaterials(transform, meshComponent, nullptr);
			return;
		}

		s_Data.GeometryShader->Bind();
		DrawModelCulled(*meshComponent.MeshModel, transform, s_Data.GeometryShader);
	}

	// ============================================================================
	// LIGHTING PASS
	// ============================================================================

	bool DeferredRenderer::IsPostProcessingActive() {
		// The composite pass always runs so exposure / tone-map operator / gamma settings
		// apply even when no other effect is enabled. The lighting pass then outputs linear HDR.
		return PostProcessRenderer::IsInitialized();
	}

	void DeferredRenderer::ExecuteLightingPass(const Ref<Framebuffer>& targetFramebuffer) {
		LNX_PROFILE_FUNCTION();

		if (!s_Data.LightingShader || !s_Data.LightingShader->IsValid()) {
			LNX_LOG_ERROR("DeferredRenderer: Lighting shader not available, skipping lighting pass");
			return;
		}

		if (!s_Data.GBuffer.IsInitialized()) {
			return;
		}

		// Bind the target framebuffer (scene FB)
		targetFramebuffer->Bind();

		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			// Only write to color attachment 0 (not entity ID at attachment 1)
			// This prevents the lighting quad from clobbering entity IDs
			cmdList->SetDrawBuffers({ 0 });

			// Disable depth test for full-screen quad
			cmdList->SetDepthTestEnabled(false);
			cmdList->SetDepthMask(false);
		}

		// ? FIX: Disable blending for the lighting pass.
		glDisable(GL_BLEND);

		// Sky lighting/reflections (IBL). Bound here every frame: the deferred path used to
		// rely on whatever state Renderer3D left behind, and the material preview renderer
		// unbinds the environment, which turned sky reflections off for the whole scene.
		{
			auto environment = SkyboxRenderer::GetGlobalEnvironment();
			if (environment && environment->IsLoaded()) Renderer3D::BindEnvironment(environment);
			else Renderer3D::UnbindEnvironment();
		}

		// Bind the lighting shader
		s_Data.LightingShader->Bind();

		// When post-processing is active, output linear HDR (composite pass handles tone mapping)
		// When inactive, the lighting shader applies tone mapping + gamma itself
		bool postProcessActive = IsPostProcessingActive();
		s_Data.PostProcessControlBuffer.SkipToneMapGamma = postProcessActive ? 1 : 0;
		s_Data.PostProcessControlBuffer._pad1 = 0.0f;
		s_Data.PostProcessControlBuffer._pad2 = 0.0f;
		s_Data.PostProcessControlBuffer._pad3 = 0.0f;
		s_Data.PostProcessControlUniformBuffer->SetData(&s_Data.PostProcessControlBuffer, sizeof(DeferredRendererData::PostProcessControlData));

		// Bind G-Buffer textures for sampling
		s_Data.LightingShader->SetInt("gAlbedoMetallic",   0);
		s_Data.LightingShader->SetInt("gNormalRoughness",  1);
		s_Data.LightingShader->SetInt("gEmissionAO",       2);
		s_Data.LightingShader->SetInt("gPositionSpecular", 3);
		s_Data.LightingShader->SetInt("gDepth",            4);

		// Bind G-Buffer textures to sampler units via RHI
		auto* rhiFB = s_Data.GBuffer.GetFramebuffer()->GetRHIFramebuffer();
		if (rhiFB) {
			auto albedoMet = rhiFB->GetColorAttachment(static_cast<uint32_t>(GBufferAttachment::AlbedoMetallic));
			auto normalRough = rhiFB->GetColorAttachment(static_cast<uint32_t>(GBufferAttachment::NormalRoughness));
			auto emissionAO = rhiFB->GetColorAttachment(static_cast<uint32_t>(GBufferAttachment::EmissionAO));
			auto posSpec = rhiFB->GetColorAttachment(static_cast<uint32_t>(GBufferAttachment::PositionSpecular));
			auto depth = rhiFB->GetDepthAttachment();
			auto subsurface = rhiFB->GetColorAttachment(static_cast<uint32_t>(GBufferAttachment::Subsurface));

			if (albedoMet)   albedoMet->Bind(0);
			if (normalRough) normalRough->Bind(1);
			if (emissionAO)  emissionAO->Bind(2);
			if (posSpec)     posSpec->Bind(3);
			if (depth)       depth->Bind(4);
			if (subsurface)  subsurface->Bind(5);
		}

		// Draw full-screen quad (6 vertices = 2 triangles)
		s_Data.QuadVAO->Bind();
		if (cmdList) {
			cmdList->DrawArrays(6);
		}

		s_Data.Stats.LightingDrawCalls++;

		// Restore depth state and draw buffers
		if (cmdList) {
			cmdList->SetDepthTestEnabled(true);
			cmdList->SetDepthMask(true);
			// Restore all draw buffers for subsequent rendering (skybox, billboards, etc.)
			cmdList->SetDrawBuffers({ 0, 1 });
		}

		// Copy ONLY depth from G-Buffer to target framebuffer for forward pass elements (skybox, grid, etc.)
		if (rhiFB && targetFramebuffer->GetRHIFramebuffer()) {
			auto* glSrc = dynamic_cast<RHI::OpenGLRHIFramebuffer*>(rhiFB);
			auto* glDst = dynamic_cast<RHI::OpenGLRHIFramebuffer*>(targetFramebuffer->GetRHIFramebuffer());
			if (glSrc && glDst) {
				glBlitNamedFramebuffer(
					glSrc->GetFramebufferID(), glDst->GetFramebufferID(),
					0, 0, s_Data.GBuffer.GetWidth(), s_Data.GBuffer.GetHeight(),
					0, 0, targetFramebuffer->GetSpecification().Width, targetFramebuffer->GetSpecification().Height,
					GL_DEPTH_BUFFER_BIT,
					GL_NEAREST
				);
			}
		}

		targetFramebuffer->Bind();
	}

	// ============================================================================
	// POST-PROCESSING PASS (called AFTER all overlays are rendered)
	// ============================================================================

	void DeferredRenderer::ExecutePostProcessing(const Ref<Framebuffer>& targetFramebuffer) {
		LNX_PROFILE_FUNCTION();

		if (!IsPostProcessingActive()) return;

		// Get the scene color texture that contains the complete scene (lighting + overlays)
		uint32_t sceneColorTexID = targetFramebuffer->GetColorAttachmentRendererID(0);
		uint32_t width = targetFramebuffer->GetSpecification().Width;
		uint32_t height = targetFramebuffer->GetSpecification().Height;

		PostProcessFrameInputs inputs;
		inputs.AlbedoMetallicTex   = s_Data.GBuffer.GetAttachmentRendererID(GBufferAttachment::AlbedoMetallic);
		inputs.NormalRoughnessTex  = s_Data.GBuffer.GetAttachmentRendererID(GBufferAttachment::NormalRoughness);
		inputs.PositionSpecularTex = s_Data.GBuffer.GetAttachmentRendererID(GBufferAttachment::PositionSpecular);
		inputs.DepthTex            = s_Data.GBuffer.GetDepthAttachmentRendererID();
		inputs.View                = s_Data.ViewMatrix;
		inputs.Projection          = s_Data.ProjectionMatrix;
		PostProcessRenderer::SetFrameInputs(inputs);

		PostProcessRenderer::Execute(sceneColorTexID, targetFramebuffer, width, height);

		targetFramebuffer->Bind();
	}

	// ============================================================================
	// ENVIRONMENT
	// ============================================================================

	void DeferredRenderer::BindEnvironment(const Ref<EnvironmentMap>& environment) {
		if (!environment || !environment->IsLoaded()) {
			UnbindEnvironment();
			return;
		}

		if (environment->GetIrradianceMap()) {
			environment->GetIrradianceMap()->Bind(8);
		}
		if (environment->GetPrefilteredMap()) {
			environment->GetPrefilteredMap()->Bind(9);
		}
		if (environment->GetBRDFLUT()) {
			environment->GetBRDFLUT()->Bind(10);
		}
	}

	void DeferredRenderer::UnbindEnvironment() {
		// Environment textures at 8, 9, 10 will just be left unbound
	}

	// ============================================================================
	// G-BUFFER ACCESS
	// ============================================================================

	GBuffer& DeferredRenderer::GetGBuffer() {
		return s_Data.GBuffer;
	}

	void DeferredRenderer::OnViewportResize(uint32_t width, uint32_t height) {
		if (!s_Data.GBuffer.IsInitialized()) {
			s_Data.GBuffer.Initialize(width, height);
		} else {
			s_Data.GBuffer.Resize(width, height);
		}
		PostProcessRenderer::OnViewportResize(width, height);
	}

	int DeferredRenderer::ReadEntityID(int x, int y) {
		return s_Data.GBuffer.ReadEntityID(x, y);
	}

	// ============================================================================
	// STATISTICS
	// ============================================================================

	void DeferredRenderer::ResetStats() {
		memset(&s_Data.Stats, 0, sizeof(DeferredRenderer::Statistics));
	}

	DeferredRenderer::Statistics DeferredRenderer::GetStats() {
		return s_Data.Stats;
	}

} // namespace Lunex