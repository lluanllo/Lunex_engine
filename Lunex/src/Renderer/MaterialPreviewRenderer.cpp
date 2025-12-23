#include "stpch.h"
#include "MaterialPreviewRenderer.h"
#include "Renderer/Renderer3D.h"
#include "RHI/RHI.h"
#include "Resources/Render/MaterialInstance.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <fstream>

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

	// ========== CONFIGURATION ==========

	void MaterialPreviewRenderer::SetResolution(uint32_t width, uint32_t height) {
		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;
		InitializeFramebuffer();

		m_Camera.SetViewportSize(static_cast<float>(width), static_cast<float>(height));
	}

	void MaterialPreviewRenderer::SetPreviewModel(Ref<Model> model) {
		if (!model) {
			LNX_LOG_WARN("MaterialPreviewRenderer::SetPreviewModel - Model is null, using default sphere");
			m_PreviewModel = Model::CreateSphere();
			return;
		}
		m_PreviewModel = model;
	}

	void MaterialPreviewRenderer::SetCameraPosition(const glm::vec3& position) {
		m_Camera.SetPosition(position);
	}

	glm::vec3 MaterialPreviewRenderer::GetCameraPosition() const {
		return m_Camera.GetPosition();
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

		RenderInternal(material);
		return CopyFramebufferToTexture();
	}

	Ref<Texture2D> MaterialPreviewRenderer::CopyFramebufferToTexture() {
		if (!m_Framebuffer) {
			return nullptr;
		}

		Ref<Texture2D> texture = Texture2D::Create(m_Width, m_Height);
		if (!texture) {
			return nullptr;
		}

		uint32_t dataSize = m_Width * m_Height * 4;
		std::vector<uint8_t> pixels(dataSize);

		m_Framebuffer->Bind();
		glReadPixels(0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		m_Framebuffer->Unbind();

		texture->SetData(pixels.data(), dataSize);

		return texture;
	}

	Ref<Texture2D> MaterialPreviewRenderer::GenerateThumbnail(Ref<MaterialAsset> material, uint32_t size) {
		uint32_t oldWidth = m_Width;
		uint32_t oldHeight = m_Height;
		bool oldAutoRotate = m_AutoRotate;

		SetResolution(size, size);
		SetAutoRotate(false);

		Ref<Texture2D> thumbnail = RenderToTexture(material);

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

	// ========== PRIVATE HELPERS ==========

	void MaterialPreviewRenderer::InitializeFramebuffer() {
		FramebufferSpecification spec;
		spec.Width = m_Width;
		spec.Height = m_Height;
		spec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		spec.Samples = 1;

		m_Framebuffer = Framebuffer::Create(spec);
	}

	void MaterialPreviewRenderer::InitializePreviewScene() {
		m_PreviewModel = Model::CreateSphere();

		m_Camera = EditorCamera(45.0f, 1.0f, 0.1f, 1000.0f);
		m_Camera.SetViewportSize(static_cast<float>(m_Width), static_cast<float>(m_Height));
		m_Camera.SetPosition(glm::vec3(0.0f, 0.0f, 2.5f));

		m_PreviewScene = CreateRef<Scene>();

		Entity lightEntity = m_PreviewScene->CreateEntity("Preview Light");
		auto& lightComp = lightEntity.AddComponent<LightComponent>(LightType::Directional);
		auto& lightTransform = lightEntity.GetComponent<TransformComponent>();
		
		lightTransform.Translation = glm::vec3(2.0f, 3.0f, 2.0f);
		lightTransform.Rotation = glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f);
		
		lightComp.SetColor(m_LightColor);
		lightComp.SetIntensity(m_LightIntensity);

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

		if (!material || !m_Framebuffer) {
			return;
		}

		auto* cmdList = RHI::GetImmediateCommandList();
		if (!cmdList) return;

		// ========== BIND FRAMEBUFFER ==========
		m_Framebuffer->Bind();

		cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));
		cmdList->SetClearColor(m_BackgroundColor);
		cmdList->Clear();

		m_Framebuffer->ClearAttachment(1, -1);

		// ========== USE RENDERER3D ==========
		// Calculate view matrix for material preview camera
		glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 2.5f);
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::mat4 viewMatrix = glm::lookAt(cameraPos, target, glm::vec3(0, 1, 0));
		glm::mat4 cameraTransform = glm::inverse(viewMatrix);
		
		// Create a temporary SceneCamera for the preview
		SceneCamera previewCamera;
		previewCamera.SetPerspective(45.0f, 0.1f, 100.0f);
		previewCamera.SetViewportSize(m_Width, m_Height);
		
		// Begin scene with preview camera
		Renderer3D::BeginScene(previewCamera, cameraTransform);
		Renderer3D::UpdateLights(m_PreviewScene.get());

		// Calculate transform with rotation
		glm::mat4 transform = glm::mat4(1.0f);
		if (m_AutoRotate) {
			transform = glm::rotate(transform, glm::radians(m_CurrentRotation), glm::vec3(0.0f, 1.0f, 0.0f));
		}

		// ========== CREATE TEMPORARY MaterialComponent FROM MaterialAsset ==========
		// This is the KEY FIX: we create a proper MaterialComponent with the MaterialAsset
		// so that Renderer3D::DrawModel uses all PBR properties, not just color
		MaterialComponent tempMaterial(material);
		
		// Draw the preview model with full PBR material
		Renderer3D::DrawModel(transform, m_PreviewModel, tempMaterial, -1);

		Renderer3D::EndScene();

		// ========== UNBIND FRAMEBUFFER ==========
		m_Framebuffer->Unbind();
	}

	// ========== THUMBNAIL DISK CACHING ==========

	Ref<Texture2D> MaterialPreviewRenderer::GetOrGenerateCachedThumbnail(const std::filesystem::path& materialPath, Ref<MaterialAsset> material) {
		if (!material) {
			return nullptr;
		}

		std::filesystem::path thumbnailPath = GetThumbnailPath(materialPath);

		if (std::filesystem::exists(thumbnailPath) && IsThumbnailValid(thumbnailPath, materialPath)) {
			Ref<Texture2D> thumbnail = LoadThumbnailFromDisk(thumbnailPath);
			if (thumbnail) {
				LNX_LOG_TRACE("Loaded thumbnail from cache: {0}", thumbnailPath.filename().string());
				return thumbnail;
			}
		}

		Ref<Texture2D> thumbnail = RenderToTexture(material);
		if (thumbnail) {
			if (SaveThumbnailToDisk(thumbnailPath, thumbnail)) {
				LNX_LOG_TRACE("Generated and cached thumbnail: {0}", thumbnailPath.filename().string());
			}
		}

		return thumbnail;
	}

	void MaterialPreviewRenderer::InvalidateCachedThumbnail(const std::filesystem::path& materialPath) {
		std::filesystem::path thumbnailPath = GetThumbnailPath(materialPath);
		if (std::filesystem::exists(thumbnailPath)) {
			try {
				std::filesystem::remove(thumbnailPath);
				LNX_LOG_TRACE("Invalidated thumbnail cache: {0}", thumbnailPath.filename().string());
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to remove cached thumbnail: {0}", e.what());
			}
		}
	}

	void MaterialPreviewRenderer::ClearThumbnailCache() {
		std::filesystem::path cacheDir = GetThumbnailCachePath();
		if (std::filesystem::exists(cacheDir)) {
			try {
				std::filesystem::remove_all(cacheDir);
				std::filesystem::create_directories(cacheDir);
				LNX_LOG_INFO("Cleared thumbnail cache directory");
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to clear thumbnail cache: {0}", e.what());
			}
		}
	}

	// ========== DISK CACHE HELPERS ==========

	std::filesystem::path MaterialPreviewRenderer::GetThumbnailCachePath() const {
		std::filesystem::path cacheDir = "Cache/Thumbnails";
		
		if (!std::filesystem::exists(cacheDir)) {
			std::filesystem::create_directories(cacheDir);
		}
		
		return cacheDir;
	}

	std::filesystem::path MaterialPreviewRenderer::GetThumbnailPath(const std::filesystem::path& materialPath) const {
		std::string filename = materialPath.filename().stem().string() + ".png";
		
		if (materialPath.has_parent_path()) {
			std::string parentName = materialPath.parent_path().filename().string();
			if (!parentName.empty() && parentName != "assets") {
				filename = parentName + "_" + filename;
			}
		}
		
		return GetThumbnailCachePath() / filename;
	}

	bool MaterialPreviewRenderer::IsThumbnailValid(const std::filesystem::path& thumbnailPath, const std::filesystem::path& materialPath) const {
		if (!std::filesystem::exists(thumbnailPath) || !std::filesystem::exists(materialPath)) {
			return false;
		}

		auto thumbnailTime = std::filesystem::last_write_time(thumbnailPath);
		auto materialTime = std::filesystem::last_write_time(materialPath);

		return thumbnailTime >= materialTime;
	}

	bool MaterialPreviewRenderer::SaveThumbnailToDisk(const std::filesystem::path& thumbnailPath, Ref<Texture2D> thumbnail) {
		if (!thumbnail) {
			return false;
		}

		uint32_t dataSize = thumbnail->GetWidth() * thumbnail->GetHeight() * 4;
		std::vector<uint8_t> pixels(dataSize);
		
		glBindTexture(GL_TEXTURE_2D, thumbnail->GetRendererID());
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);

		uint32_t width = thumbnail->GetWidth();
		uint32_t height = thumbnail->GetHeight();
		uint32_t rowSize = width * 4;
		std::vector<uint8_t> flipped(dataSize);
		
		for (uint32_t y = 0; y < height; y++) {
			memcpy(flipped.data() + y * rowSize, 
			       pixels.data() + (height - 1 - y) * rowSize, 
			       rowSize);
		}

		int result = stbi_write_png(thumbnailPath.string().c_str(), 
		                             width, height, 4, 
		                             flipped.data(), width * 4);

		if (result == 0) {
			LNX_LOG_ERROR("Failed to save thumbnail to disk: {0}", thumbnailPath.string());
			return false;
		}

		return true;
	}

	Ref<Texture2D> MaterialPreviewRenderer::LoadThumbnailFromDisk(const std::filesystem::path& thumbnailPath) {
		if (!std::filesystem::exists(thumbnailPath)) {
			return nullptr;
		}

		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		unsigned char* data = stbi_load(thumbnailPath.string().c_str(), &width, &height, &channels, 4);

		if (!data) {
			LNX_LOG_ERROR("Failed to load thumbnail from disk: {0}", thumbnailPath.string());
			return nullptr;
		}

		Ref<Texture2D> texture = Texture2D::Create(width, height);
		if (texture) {
			texture->SetData(data, width * height * 4);
		}

		stbi_image_free(data);

		return texture;
	}

}
