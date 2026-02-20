#include "stpch.h"
#include "DeferredRenderer.h"

#include "RHI/RHI.h"
#include "RHI/OpenGL/OpenGLRHIFramebuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Renderer/SkyboxRenderer.h"
#include "Renderer/Shadows/ShadowSystem.h"
#include "Renderer/PostProcess/PostProcessRenderer.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Resources/Mesh/Model.h"
#include "Scene/Components.h"
#include "Scene/Lighting/LightTypes.h"
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

		struct TransformData {
			glm::mat4 Transform;
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

		ShadowSystem::Get().BindForSceneRendering();
	}

	void DeferredRenderer::EndScene() {
		LNX_PROFILE_FUNCTION();
	}

	// ============================================================================
	// GEOMETRY PASS
	// ============================================================================

	void DeferredRenderer::BeginGeometryPass() {
		LNX_PROFILE_FUNCTION();

		if (!s_Data.GBuffer.IsInitialized()) {
			return;
		}

		s_Data.GBuffer.Bind();

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

	void DeferredRenderer::SubmitMesh(const glm::mat4& transform, MeshComponent& meshComponent,
									  MaterialComponent& materialComponent, int entityID) {
		if (!meshComponent.MeshModel || !materialComponent.Instance)
			return;

		if (!s_Data.GeometryShader || !s_Data.GeometryShader->IsValid() || !s_Data.GBuffer.IsInitialized())
			return;

		const_cast<Model*>(meshComponent.MeshModel.get())->SetEntityID(entityID);

		// Update Transform UBO
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(DeferredRendererData::TransformData));

		// Get material uniform data
		auto uniformData = materialComponent.Instance->GetUniformData();

		// Fill material buffer
		s_Data.MaterialBuffer.Color = uniformData.Albedo;
		s_Data.MaterialBuffer.Metallic = uniformData.Metallic;
		s_Data.MaterialBuffer.Roughness = uniformData.Roughness;
		s_Data.MaterialBuffer.Specular = uniformData.Specular;
		s_Data.MaterialBuffer.EmissionIntensity = uniformData.EmissionIntensity;
		s_Data.MaterialBuffer.EmissionColor = uniformData.EmissionColor;
		s_Data.MaterialBuffer.NormalIntensity = uniformData.NormalIntensity;
		s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

		s_Data.MaterialBuffer.UseAlbedoMap = uniformData.UseAlbedoMap;
		s_Data.MaterialBuffer.UseNormalMap = uniformData.UseNormalMap;
		s_Data.MaterialBuffer.UseMetallicMap = uniformData.UseMetallicMap;
		s_Data.MaterialBuffer.UseRoughnessMap = uniformData.UseRoughnessMap;
		s_Data.MaterialBuffer.UseSpecularMap = uniformData.UseSpecularMap;
		s_Data.MaterialBuffer.UseEmissionMap = uniformData.UseEmissionMap;
		s_Data.MaterialBuffer.UseAOMap = uniformData.UseAOMap;

		s_Data.MaterialBuffer.MetallicMultiplier = uniformData.MetallicMultiplier;
		s_Data.MaterialBuffer.RoughnessMultiplier = uniformData.RoughnessMultiplier;
		s_Data.MaterialBuffer.SpecularMultiplier = uniformData.SpecularMultiplier;
		s_Data.MaterialBuffer.AOMultiplier = uniformData.AOMultiplier;

		s_Data.MaterialBuffer.DetailNormalCount = uniformData.DetailNormalCount;
		s_Data.MaterialBuffer.UseLayeredTexture = uniformData.UseLayeredTexture;
		s_Data.MaterialBuffer.LayeredMetallicChannel = uniformData.LayeredMetallicChannel;
		s_Data.MaterialBuffer.LayeredRoughnessChannel = uniformData.LayeredRoughnessChannel;
		s_Data.MaterialBuffer.LayeredAOChannel = uniformData.LayeredAOChannel;
		s_Data.MaterialBuffer.LayeredUseMetallic = uniformData.LayeredUseMetallic;
		s_Data.MaterialBuffer.LayeredUseRoughness = uniformData.LayeredUseRoughness;
		s_Data.MaterialBuffer.LayeredUseAO = uniformData.LayeredUseAO;
		s_Data.MaterialBuffer.DetailNormalIntensities = uniformData.DetailNormalIntensities;
		s_Data.MaterialBuffer.DetailNormalTilingX = uniformData.DetailNormalTilingX;
		s_Data.MaterialBuffer.DetailNormalTilingY = uniformData.DetailNormalTilingY;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(DeferredRendererData::MaterialUniformData));

		// Bind geometry shader and material textures
		s_Data.GeometryShader->Bind();
		materialComponent.Instance->BindTextures();

		// Draw
		meshComponent.MeshModel->Draw(s_Data.GeometryShader);

		s_Data.Stats.GeometryDrawCalls++;
		s_Data.Stats.MeshCount += static_cast<uint32_t>(meshComponent.MeshModel->GetMeshes().size());
		for (const auto& mesh : meshComponent.MeshModel->GetMeshes()) {
			s_Data.Stats.TriangleCount += static_cast<uint32_t>(mesh->GetIndices().size()) / 3;
		}
	}

	void DeferredRenderer::SubmitMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID) {
		if (!meshComponent.MeshModel)
			return;

		if (!s_Data.GeometryShader || !s_Data.GeometryShader->IsValid() || !s_Data.GBuffer.IsInitialized())
			return;

		const_cast<Model*>(meshComponent.MeshModel.get())->SetEntityID(entityID);

		// Update Transform UBO
		s_Data.TransformBuffer.Transform = transform;
		s_Data.TransformUniformBuffer->SetData(&s_Data.TransformBuffer, sizeof(DeferredRendererData::TransformData));

		// Default material
		memset(&s_Data.MaterialBuffer, 0, sizeof(DeferredRendererData::MaterialUniformData));
		s_Data.MaterialBuffer.Color = glm::vec4(1.0f);
		s_Data.MaterialBuffer.Roughness = 0.5f;
		s_Data.MaterialBuffer.Specular = 0.5f;
		s_Data.MaterialBuffer.NormalIntensity = 1.0f;
		s_Data.MaterialBuffer.MetallicMultiplier = 1.0f;
		s_Data.MaterialBuffer.RoughnessMultiplier = 1.0f;
		s_Data.MaterialBuffer.SpecularMultiplier = 1.0f;
		s_Data.MaterialBuffer.AOMultiplier = 1.0f;
		s_Data.MaterialBuffer.ViewPos = s_Data.CameraPosition;

		s_Data.MaterialUniformBuffer->SetData(&s_Data.MaterialBuffer, sizeof(DeferredRendererData::MaterialUniformData));

		s_Data.GeometryShader->Bind();
		meshComponent.MeshModel->Draw(s_Data.GeometryShader);

		s_Data.Stats.GeometryDrawCalls++;
		s_Data.Stats.MeshCount += static_cast<uint32_t>(meshComponent.MeshModel->GetMeshes().size());
		for (const auto& mesh : meshComponent.MeshModel->GetMeshes()) {
			s_Data.Stats.TriangleCount += static_cast<uint32_t>(mesh->GetIndices().size()) / 3;
		}
	}

	// ============================================================================
	// LIGHTING PASS
	// ============================================================================

	bool DeferredRenderer::IsPostProcessingActive() {
		auto& config = PostProcessRenderer::GetConfig();
		return PostProcessRenderer::IsInitialized() &&
			   (config.EnableBloom || config.EnableVignette || config.EnableChromaticAberration);
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

			if (albedoMet)   albedoMet->Bind(0);
			if (normalRough) normalRough->Bind(1);
			if (emissionAO)  emissionAO->Bind(2);
			if (posSpec)     posSpec->Bind(3);
			if (depth)       depth->Bind(4);
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