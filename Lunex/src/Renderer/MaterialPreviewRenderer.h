#pragma once

#include "Core/Core.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Resources/Render/MaterialInstance.h"
#include "Scene/Camera/EditorCamera.h"
#include "Scene/Camera/SceneCamera.h"
#include "resources/Mesh/Model.h"
#include "Scene/Lighting/Light.h"
#include "Renderer/Texture.h"
#include <glm/glm.hpp>
#include <filesystem>

// Forward declarations
namespace Lunex {
	class Scene;
}

namespace Lunex {

	/**
	 * @class MaterialPreviewRenderer
	 * @brief Renders material previews to an isolated framebuffer
	 * 
	 * Uses Renderer3D for rendering. The EditorLayer is responsible
	 * for restoring UBOs after this renderer finishes.
	 */
	class MaterialPreviewRenderer {
	public:
		MaterialPreviewRenderer();
		~MaterialPreviewRenderer();

		// ========== CONFIGURATION ==========

		void SetResolution(uint32_t width, uint32_t height);
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		void SetPreviewModel(Ref<Model> model);
		Ref<Model> GetPreviewModel() const { return m_PreviewModel; }

		void SetCameraPosition(const glm::vec3& position);
		glm::vec3 GetCameraPosition() const;

		void SetLightIntensity(float intensity) { m_LightIntensity = intensity; }
		void SetLightColor(const glm::vec3& color) { m_LightColor = color; }
		void SetBackgroundColor(const glm::vec4& color) { m_BackgroundColor = color; }
		glm::vec4 GetBackgroundColor() const { return m_BackgroundColor; }

		void SetAutoRotate(bool enabled) { m_AutoRotate = enabled; }
		bool GetAutoRotate() const { return m_AutoRotate; }
		void SetRotationSpeed(float speed) { m_RotationSpeed = speed; }
		float GetRotationSpeed() const { return m_RotationSpeed; }

		// ========== RENDERING ==========

		void RenderPreview(Ref<MaterialAsset> material);
		void RenderPreview(Ref<MaterialInstance> materialInstance);

		uint32_t GetPreviewTextureID() const;
		Ref<Texture2D> GetPreviewTexture() const { return m_PreviewTexture; }

		// ========== THUMBNAIL GENERATION ==========

		Ref<Texture2D> RenderToTexture(Ref<MaterialAsset> material);
		Ref<Texture2D> GenerateThumbnail(Ref<MaterialAsset> material, uint32_t size = 256);

		Ref<Texture2D> GetOrGenerateCachedThumbnail(const std::filesystem::path& materialPath, Ref<MaterialAsset> material);
		void InvalidateCachedThumbnail(const std::filesystem::path& materialPath);
		void ClearThumbnailCache();

		// ========== UPDATE ==========

		void Update(float deltaTime);

	private:
		// Rendering resources
		Ref<Framebuffer> m_Framebuffer;
		Ref<Texture2D> m_PreviewTexture;

		uint32_t m_Width = 512;
		uint32_t m_Height = 512;

		Ref<Model> m_PreviewModel;
		Ref<Scene> m_PreviewScene;
		EditorCamera m_Camera;

		Ref<Light> m_MainLight;
		glm::vec3 m_LightColor = glm::vec3(1.0f);
		float m_LightIntensity = 1.0f;

		glm::vec4 m_BackgroundColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

		bool m_AutoRotate = true;
		float m_RotationSpeed = 20.0f;
		float m_CurrentRotation = 0.0f;

		void InitializeFramebuffer();
		void InitializePreviewScene();
		void RenderInternal(Ref<MaterialAsset> material);
		Ref<Texture2D> CopyFramebufferToTexture();

		std::filesystem::path GetThumbnailCachePath() const;
		std::filesystem::path GetThumbnailPath(const std::filesystem::path& materialPath) const;
		bool IsThumbnailValid(const std::filesystem::path& thumbnailPath, const std::filesystem::path& materialPath) const;
		bool SaveThumbnailToDisk(const std::filesystem::path& thumbnailPath, Ref<Texture2D> thumbnail);
		Ref<Texture2D> LoadThumbnailFromDisk(const std::filesystem::path& thumbnailPath);
	};

}
