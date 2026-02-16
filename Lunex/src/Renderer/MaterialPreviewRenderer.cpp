#include "stpch.h"
#include "MaterialPreviewRenderer.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/Shadows/ShadowSystem.h"
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

	// ? NEW: Camera positioning methods
	void MaterialPreviewRenderer::SetCameraPosition(const glm::vec3& position) {
		m_Camera.SetPosition(position);
	}

	glm::vec3 MaterialPreviewRenderer::GetCameraPosition() const {
		return m_Camera.GetPosition();
	}

	glm::vec3 MaterialPreviewRenderer::ComputeCameraForBounds(const glm::vec3& boundsMin, const glm::vec3& boundsMax, float fovDegrees) {
		glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
		glm::vec3 extents = boundsMax - boundsMin;
		float radius = glm::length(extents) * 0.5f;
		
		if (radius < 0.001f) radius = 1.0f;
		
		float fovRad = glm::radians(fovDegrees);
		float distance = (radius / std::sin(fovRad * 0.5f)) * 1.1f;
		
		// Position camera at a 30-degree elevation angle and slightly to the right
		float elevation = glm::radians(20.0f);
		float azimuth = glm::radians(30.0f);
		
		glm::vec3 cameraOffset;
		cameraOffset.x = distance * std::cos(elevation) * std::sin(azimuth);
		cameraOffset.y = center.y + distance * std::sin(elevation);
		cameraOffset.z = distance * std::cos(elevation) * std::cos(azimuth);
		
		return glm::vec3(center.x + cameraOffset.x, cameraOffset.y, center.z + cameraOffset.z);
	}

	void MaterialPreviewRenderer::ComputeModelBounds(const Ref<Model>& model, glm::vec3& outMin, glm::vec3& outMax) {
		outMin = glm::vec3(std::numeric_limits<float>::max());
		outMax = glm::vec3(std::numeric_limits<float>::lowest());
		
		if (!model) return;
		
		for (const auto& mesh : model->GetMeshes()) {
			for (const auto& vertex : mesh->GetVertices()) {
				outMin = glm::min(outMin, vertex.Position);
				outMax = glm::max(outMax, vertex.Position);
			}
		}
		
		// Fallback if no vertices found
		if (outMin.x > outMax.x) {
			outMin = glm::vec3(-0.5f);
			outMax = glm::vec3(0.5f);
		}
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

		// ? Camera settings for material preview (frontal, centered view)
		m_Camera = EditorCamera(45.0f, 1.0f, 0.1f, 1000.0f);
		m_Camera.SetViewportSize(m_Width, m_Height);
		
		// ? Frontal centered position for materials (sphere radius=0.5f)
		// Camera directly in front at z=2.5f looking at origin
		m_Camera.SetPosition(glm::vec3(0.0f, 0.0f, 2.5f));

		// Crear escena temporal para el preview
		m_PreviewScene = CreateRef<Scene>();

		// ? Luz principal para materials (centrada, intensa)
		Entity lightEntity = m_PreviewScene->CreateEntity("Preview Light");
		auto& lightComp = lightEntity.AddComponent<LightComponent>(LightType::Directional);
		auto& lightTransform = lightEntity.GetComponent<TransformComponent>();
		
		lightTransform.Translation = glm::vec3(2.0f, 3.0f, 2.0f);
		lightTransform.Rotation = glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f);
		
		lightComp.SetColor(m_LightColor);
		lightComp.SetIntensity(m_LightIntensity);

		// Luz de relleno
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

		auto* cmdList = RHI::GetImmediateCommandList();
		if (!cmdList) {
			LNX_LOG_ERROR("MaterialPreviewRenderer::RenderInternal - No immediate command list available");
			return;
		}

		// ============================================================
		// SAVE CURRENT STATE before modifying anything
		// This prevents the preview from corrupting the main scene rendering
		// ============================================================
		
		// Save current viewport
		int previousViewport[4];
		cmdList->GetViewport(previousViewport);
		
		// Save current framebuffer binding
		uint64_t previousFramebuffer = cmdList->GetBoundFramebuffer();

		// ============================================================
		// TEMPORARILY DISABLE SHADOW SYSTEM
		// ============================================================
		auto& shadowSystem = ShadowSystem::Get();
		bool previousShadowEnabled = shadowSystem.IsEnabled();
		shadowSystem.SetEnabled(false);

		// ============================================================
		// RENDER PREVIEW TO ISOLATED FRAMEBUFFER
		// ============================================================

		// Bind our isolated framebuffer
		m_Framebuffer->Bind();

		// Set viewport to match our framebuffer size
		cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));

		// Clear with background color
		cmdList->SetClearColor(m_BackgroundColor);
		cmdList->Clear();

		// Clear entity ID attachment to -1 (attachment index 1)
		m_Framebuffer->ClearAttachment(1, -1);

		// Begin scene with our isolated preview camera
		Renderer3D::BeginScene(m_Camera);

		// Disable IBL to prevent main scene's environment map from contaminating the preview
		Renderer3D::UnbindEnvironment();

		// Update lights from preview scene (isolated)
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
			
			// Restore state even on error
			m_Framebuffer->Unbind();
			cmdList->BindFramebufferByHandle(previousFramebuffer);
			cmdList->SetViewport(static_cast<float>(previousViewport[0]), static_cast<float>(previousViewport[1]),
								 static_cast<float>(previousViewport[2]), static_cast<float>(previousViewport[3]));
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

		// Unbind our framebuffer
		m_Framebuffer->Unbind();

		// ============================================================
		// RESTORE PREVIOUS STATE
		// ============================================================
		
		// Restore previous framebuffer
		cmdList->BindFramebufferByHandle(previousFramebuffer);
		
		// Restore previous viewport
		cmdList->SetViewport(static_cast<float>(previousViewport[0]), static_cast<float>(previousViewport[1]),
							 static_cast<float>(previousViewport[2]), static_cast<float>(previousViewport[3]));

		// ============================================================
		// RESTORE SHADOW SETTINGS
		// ============================================================
		
		shadowSystem.SetEnabled(previousShadowEnabled);
	}

	// ========== THUMBNAIL DISK CACHING ==========

	Ref<Texture2D> MaterialPreviewRenderer::GetOrGenerateCachedThumbnail(const std::filesystem::path& materialPath, Ref<MaterialAsset> material) {
		if (!material) {
			return nullptr;
		}

		// Get thumbnail cache path
		std::filesystem::path thumbnailPath = GetThumbnailPath(materialPath);

		// Check if thumbnail exists and is valid (not older than material file)
		if (std::filesystem::exists(thumbnailPath) && IsThumbnailValid(thumbnailPath, materialPath)) {
			// Load from disk
			Ref<Texture2D> thumbnail = LoadThumbnailFromDisk(thumbnailPath);
			if (thumbnail) {
				LNX_LOG_TRACE("Loaded thumbnail from cache: {0}", thumbnailPath.filename().string());
				return thumbnail;
			}
		}

		// Generate new thumbnail
		Ref<Texture2D> thumbnail = RenderToTexture(material);
		if (thumbnail) {
			// Save to disk cache
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
		// Cache thumbnails in "Cache/Thumbnails" folder relative to assets
		std::filesystem::path cacheDir = "Cache/Thumbnails";
		
		// Create directory if it doesn't exist
		if (!std::filesystem::exists(cacheDir)) {
			std::filesystem::create_directories(cacheDir);
		}
		
		return cacheDir;
	}

	std::filesystem::path MaterialPreviewRenderer::GetThumbnailPath(const std::filesystem::path& materialPath) const {
		// Generate a unique filename based on material path
		// Replace slashes with underscores and change extension to .png
		std::string filename = materialPath.filename().stem().string() + ".png";
		
		// If material is in a subfolder, include folder name to avoid collisions
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

		// Check if thumbnail is newer than material file
		auto thumbnailTime = std::filesystem::last_write_time(thumbnailPath);
		auto materialTime = std::filesystem::last_write_time(materialPath);

		return thumbnailTime >= materialTime;
	}

	bool MaterialPreviewRenderer::SaveThumbnailToDisk(const std::filesystem::path& thumbnailPath, Ref<Texture2D> thumbnail) {
		if (!thumbnail) {
			return false;
		}

		// Get pixel data from texture
		uint32_t dataSize = thumbnail->GetWidth() * thumbnail->GetHeight() * 4; // RGBA
		std::vector<uint8_t> pixels(dataSize);
		
		// Read pixels from GPU
		glBindTexture(GL_TEXTURE_2D, thumbnail->GetRendererID());
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);

		// Flip image vertically (OpenGL texture is upside down)
		uint32_t width = thumbnail->GetWidth();
		uint32_t height = thumbnail->GetHeight();
		uint32_t rowSize = width * 4;
		std::vector<uint8_t> flipped(dataSize);
		
		for (uint32_t y = 0; y < height; y++) {
			memcpy(flipped.data() + y * rowSize, 
			       pixels.data() + (height - 1 - y) * rowSize, 
			       rowSize);
		}

		// Save to PNG using stb_image_write
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

		// Load image using stb_image
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1); // Flip vertically to match OpenGL
		unsigned char* data = stbi_load(thumbnailPath.string().c_str(), &width, &height, &channels, 4); // Force RGBA

		if (!data) {
			LNX_LOG_ERROR("Failed to load thumbnail from disk: {0}", thumbnailPath.string());
			return nullptr;
		}

		// Create texture
		Ref<Texture2D> texture = Texture2D::Create(width, height);
		if (texture) {
			texture->SetData(data, width * height * 4);
		}

		// Free image data
		stbi_image_free(data);

		return texture;
	}

}
