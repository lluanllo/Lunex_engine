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
#include <glad/glad.h>

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

	Ref<Texture2D> MaterialPreviewRenderer::RenderToTexture(Ref<MaterialAsset> material) {
		if (!material) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderToTexture - Material is null");
			return nullptr;
		}

		// Render the material
		RenderInternal(material);
		
		// Copy framebuffer to standalone texture
		return CopyFramebufferToTexture();
	}

	Ref<Texture2D> MaterialPreviewRenderer::CopyFramebufferToTexture() {
		if (!m_Framebuffer) {
			return nullptr;
		}

		// Create a new texture with the same dimensions
		Ref<Texture2D> texture = Texture2D::Create(m_Width, m_Height);
		if (!texture) {
			return nullptr;
		}

		// Allocate buffer for pixel data
		uint32_t dataSize = m_Width * m_Height * 4; // RGBA
		std::vector<uint8_t> pixels(dataSize);

		// Bind framebuffer and read pixels
		m_Framebuffer->Bind();
		glReadPixels(0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		m_Framebuffer->Unbind();

		// Upload to texture
		texture->SetData(pixels.data(), dataSize);

		return texture;
	}

	Ref<Texture2D> MaterialPreviewRenderer::GenerateThumbnail(Ref<MaterialAsset> material, uint32_t size) {
		// Guardar configuración actual
		uint32_t oldWidth = m_Width;
		uint32_t oldHeight = m_Height;
		bool oldAutoRotate = m_AutoRotate;

		// Configurar para thumbnail
		SetResolution(size, size);
		SetAutoRotate(false);

		// Renderizar y copiar a textura independiente
		Ref<Texture2D> thumbnail = RenderToTexture(material);

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
	}

	void MaterialPreviewRenderer::InitializePreviewScene() {
		// Crear esfera por defecto
		m_PreviewModel = Model::CreateSphere();

		// Initialize camera with proper perspective settings
		m_Camera = EditorCamera(45.0f, 1.0f, 0.1f, 1000.0f);
		m_Camera.SetViewportSize(m_Width, m_Height);
		
		// Position camera outside the sphere (radius=0.5f)
		// Camera at z=2.5f looking at origin gives good view of the sphere
		m_Camera.SetPosition(glm::vec3(0.0f, 0.0f, 2.5f));

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
	}

	void MaterialPreviewRenderer::RenderInternal(Ref<MaterialAsset> material) {
		if (!m_PreviewScene || !m_PreviewModel) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderInternal - Preview scene or model not initialized");
			return;
		}

		if (!material) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderInternal - Material is null");
			return;
		}

		if (!m_Framebuffer) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderInternal - Framebuffer not initialized");
			return;
		}

		// Bind framebuffer
		m_Framebuffer->Bind();

		// Set viewport to match framebuffer size
		RenderCommand::SetViewport(0, 0, m_Width, m_Height);

		// Clear with background color
		RenderCommand::SetClearColor(m_BackgroundColor);
		RenderCommand::Clear();

		// Clear entity ID attachment to -1 (attachment index 1)
		m_Framebuffer->ClearAttachment(1, -1);

		// Begin scene with editor camera
		Renderer3D::BeginScene(m_Camera);

		// Update lights from preview scene
		Renderer3D::UpdateLights(m_PreviewScene.get());

		// Calculate transform with rotation
		glm::mat4 transform = glm::mat4(1.0f);
		if (m_AutoRotate) {
			transform = glm::rotate(transform, glm::radians(m_CurrentRotation), glm::vec3(0.0f, 1.0f, 0.0f));
		}

		// Create temporary MaterialInstance for rendering
		Ref<MaterialInstance> tempInstance = MaterialInstance::Create(material);

		if (!tempInstance) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderInternal - Failed to create MaterialInstance");
			Renderer3D::EndScene();
			m_Framebuffer->Unbind();
			return;
		}

		// Create temporary components for rendering
		MeshComponent meshComp;
		meshComp.MeshModel = m_PreviewModel;
		meshComp.Type = ModelType::Sphere;
		meshComp.Color = glm::vec4(1.0f); // White tint

		MaterialComponent matComp;
		matComp.Instance = tempInstance;
		matComp.MaterialAssetID = material->GetID();

		// Render the mesh with material
		if (meshComp.MeshModel) {
			Renderer3D::DrawMesh(transform, meshComp, matComp, -1);
		} else {
			LNX_LOG_WARN("MaterialPreviewRenderer::RenderInternal - Preview model is null");
		}

		Renderer3D::EndScene();

		// Unbind framebuffer
		m_Framebuffer->Unbind();
	}

}
