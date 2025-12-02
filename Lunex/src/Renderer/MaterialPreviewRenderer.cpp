#include "stpch.h"
#include "MaterialPreviewRenderer.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/MaterialInstance.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	MaterialPreviewRenderer::MaterialPreviewRenderer() {
		InitializeFramebuffer();
		InitializePreviewScene();
		LNX_LOG_INFO("MaterialPreviewRenderer initialized");
	}

	MaterialPreviewRenderer::~MaterialPreviewRenderer() {
		m_Framebuffer.reset();
		m_PreviewModel.reset();
		m_MainLight.reset();
		m_PreviewScene.reset();
	}

	// ========== CONFIGURACIÓN ==========

	void MaterialPreviewRenderer::SetResolution(uint32_t width, uint32_t height) {
		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;
		InitializeFramebuffer();

		// Actualizar camera aspect ratio
		m_Camera.SetViewportSize(width, height);
		LNX_LOG_INFO("MaterialPreviewRenderer resolution updated: {0}x{1}", width, height);
	}

	void MaterialPreviewRenderer::SetPreviewModel(Ref<Model> model) {
		if (!model) {
			LNX_LOG_WARN("MaterialPreviewRenderer::SetPreviewModel - Model is null, using default sphere");
			m_PreviewModel = Model::CreateSphere();
			return;
		}
		m_PreviewModel = model;
	}

	// ========== RENDERING ==========

	void MaterialPreviewRenderer::RenderPreview(Ref<MaterialAsset> material) {
		if (!material) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderPreview - Material is null");
			return;
		}

		RenderInternal(material);
	}

	void MaterialPreviewRenderer::RenderPreview(Ref<MaterialInstance> materialInstance) {
		if (!materialInstance) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderPreview - MaterialInstance is null");
			return;
		}

		// Renderizar usando el asset base (con overrides aplicados)
		RenderInternal(materialInstance->GetBaseAsset());
	}

	uint32_t MaterialPreviewRenderer::GetPreviewTextureID() const {
		if (m_Framebuffer) {
			return m_Framebuffer->GetColorAttachmentRendererID();
		}
		return 0;
	}

	// ========== THUMBNAIL GENERATION ==========

	Ref<Texture2D> MaterialPreviewRenderer::GenerateThumbnail(Ref<MaterialAsset> material, uint32_t size) {
		// Guardar configuración actual
		uint32_t oldWidth = m_Width;
		uint32_t oldHeight = m_Height;
		bool oldAutoRotate = m_AutoRotate;

		// Configurar para thumbnail
		SetResolution(size, size);
		SetAutoRotate(false);

		// Renderizar
		RenderPreview(material);

		// Crear textura a partir del framebuffer
		Ref<Texture2D> thumbnail = m_PreviewTexture;

		// Restaurar configuración
		SetResolution(oldWidth, oldHeight);
		SetAutoRotate(oldAutoRotate);

		return thumbnail;
	}

	// ========== UPDATE ==========

	void MaterialPreviewRenderer::Update(float deltaTime) {
		if (m_AutoRotate) {
			m_CurrentRotation += m_RotationSpeed * deltaTime;
			if (m_CurrentRotation >= 360.0f) {
				m_CurrentRotation -= 360.0f;
			}
		}
	}

	// ========== HELPERS PRIVADOS ==========

	void MaterialPreviewRenderer::InitializeFramebuffer() {
		FramebufferSpecification spec;
		spec.Width = m_Width;
		spec.Height = m_Height;
		spec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		spec.Samples = 1;

		m_Framebuffer = Framebuffer::Create(spec);
		LNX_LOG_INFO("MaterialPreviewRenderer framebuffer created: {0}x{1}", m_Width, m_Height);
	}

	void MaterialPreviewRenderer::InitializePreviewScene() {
		// Crear esfera por defecto
		m_PreviewModel = Model::CreateSphere();

		// Configurar cámara
		m_Camera.SetViewportSize(m_Width, m_Height);
		m_Camera.SetDistance(3.0f);

		// Crear escena temporal para el preview
		m_PreviewScene = CreateRef<Scene>();

		// Crear entidad de luz en la escena de preview
		Entity lightEntity = m_PreviewScene->CreateEntity("Preview Light");
		auto& lightComp = lightEntity.AddComponent<LightComponent>(LightType::Directional);
		auto& lightTransform = lightEntity.GetComponent<TransformComponent>();
		
		// Posicionar luz en diagonal superior
		lightTransform.Translation = glm::vec3(2.0f, 3.0f, 2.0f);
		lightTransform.Rotation = glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f);
		
		// Configurar propiedades de luz
		lightComp.SetColor(m_LightColor);
		lightComp.SetIntensity(m_LightIntensity);

		// Crear segunda luz de relleno (fill light)
		Entity fillLightEntity = m_PreviewScene->CreateEntity("Fill Light");
		auto& fillLightComp = fillLightEntity.AddComponent<LightComponent>(LightType::Directional);
		auto& fillLightTransform = fillLightEntity.GetComponent<TransformComponent>();
		
		fillLightTransform.Translation = glm::vec3(-2.0f, 1.0f, -2.0f);
		fillLightTransform.Rotation = glm::vec3(glm::radians(-30.0f), glm::radians(-45.0f), 0.0f);
		fillLightComp.SetColor(glm::vec3(0.4f, 0.4f, 0.5f));
		fillLightComp.SetIntensity(0.5f);

		m_MainLight = lightComp.LightInstance;

		LNX_LOG_INFO("MaterialPreviewRenderer scene initialized");
	}

	void MaterialPreviewRenderer::RenderInternal(Ref<MaterialAsset> material) {
		if (!m_PreviewScene || !m_PreviewModel) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderInternal - Preview scene or model not initialized");
			return;
		}

		// Bind framebuffer
		m_Framebuffer->Bind();

		// Clear
		RenderCommand::SetClearColor(m_BackgroundColor);
		RenderCommand::Clear();

		// Begin scene
		Renderer3D::BeginScene(m_Camera);

		// Actualizar luces desde la escena de preview
		Renderer3D::UpdateLights(m_PreviewScene.get());

		// Calcular transform con rotación
		glm::mat4 transform = glm::mat4(1.0f);
		if (m_AutoRotate) {
			transform = glm::rotate(transform, glm::radians(m_CurrentRotation), glm::vec3(0.0f, 1.0f, 0.0f));
		}

		// Crear MaterialInstance temporal para el rendering
		Ref<MaterialInstance> tempInstance = MaterialInstance::Create(material);

		// Crear componentes temporales para el rendering
		MeshComponent meshComp;
		meshComp.MeshModel = m_PreviewModel;
		meshComp.Type = ModelType::Sphere;

		MaterialComponent matComp;
		matComp.Instance = tempInstance;
		matComp.MaterialAssetID = material->GetID();

		// Renderizar usando el nuevo sistema
		if (tempInstance && meshComp.MeshModel) {
			// Usar DrawMesh con MaterialComponent
			Renderer3D::DrawMesh(transform, meshComp, matComp, -1);
		}

		Renderer3D::EndScene();

		// Unbind framebuffer
		m_Framebuffer->Unbind();
	}

}
