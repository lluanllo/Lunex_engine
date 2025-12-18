#pragma once

#include "Core/Core.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/MaterialAsset.h"
#include "Renderer/MaterialInstance.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/Model.h"
#include "Renderer/Light.h"
#include "Renderer/Texture.h"
#include <glm/glm.hpp>
#include <filesystem>

// Forward declarations
namespace Lunex {
	class Scene;
}

namespace Lunex {

	// MaterialPreviewRenderer - Sistema de renderizado de preview de materiales
	// Renderiza una esfera con el material en un framebuffer dedicado
	// Usado para:
	//   - Preview thumbnails en ContentBrowser
	//   - Preview en MaterialComponent del PropertiesPanel
	//   - Vista en tiempo real en MaterialEditorPanel
	class MaterialPreviewRenderer {
	public:
		MaterialPreviewRenderer();
		~MaterialPreviewRenderer();

		// ========== CONFIGURACIÓN ==========

		// Configurar resolución del preview (default: 512x512)
		void SetResolution(uint32_t width, uint32_t height);
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		// Configurar el modelo de preview (sphere, cube, plane, etc.)
		void SetPreviewModel(Ref<Model> model);
		Ref<Model> GetPreviewModel() const { return m_PreviewModel; }

		// ? NEW: Camera positioning for mesh/prefab thumbnails
		void SetCameraPosition(const glm::vec3& position);
		glm::vec3 GetCameraPosition() const;

		// Configurar iluminación del preview
		void SetLightIntensity(float intensity) { m_LightIntensity = intensity; }
		void SetLightColor(const glm::vec3& color) { m_LightColor = color; }
		void SetBackgroundColor(const glm::vec4& color) { m_BackgroundColor = color; }

		// Rotación automática del preview
		void SetAutoRotate(bool enabled) { m_AutoRotate = enabled; }
		bool GetAutoRotate() const { return m_AutoRotate; }
		void SetRotationSpeed(float speed) { m_RotationSpeed = speed; }
		float GetRotationSpeed() const { return m_RotationSpeed; }

		// ========== RENDERING ==========

		// Renderizar material en el framebuffer
		void RenderPreview(Ref<MaterialAsset> material);
		void RenderPreview(Ref<MaterialInstance> materialInstance);

		// Obtener la textura renderizada (from framebuffer - changes every render)
		uint32_t GetPreviewTextureID() const;
		Ref<Texture2D> GetPreviewTexture() const { return m_PreviewTexture; }

		// ========== THUMBNAIL GENERATION ==========

		// Render and copy to a new standalone texture (for caching)
		Ref<Texture2D> RenderToTexture(Ref<MaterialAsset> material);

		// Generar thumbnail estático (no se actualiza automáticamente)
		Ref<Texture2D> GenerateThumbnail(Ref<MaterialAsset> material, uint32_t size = 256);

		// ? NEW: Disk caching for thumbnails
		// Load thumbnail from cache or generate if missing/outdated
		Ref<Texture2D> GetOrGenerateCachedThumbnail(const std::filesystem::path& materialPath, Ref<MaterialAsset> material);
		
		// Clear disk cache for a specific material
		void InvalidateCachedThumbnail(const std::filesystem::path& materialPath);
		
		// Clear all disk cache
		void ClearThumbnailCache();

		// ========== UPDATE ==========

		// Actualizar rotación automática (llamar en update loop si autorotate está activo)
		void Update(float deltaTime);

	private:
		// Framebuffer para renderizado offscreen
		Ref<Framebuffer> m_Framebuffer;
		Ref<Texture2D> m_PreviewTexture;

		uint32_t m_Width = 512;
		uint32_t m_Height = 512;

		// Modelo de preview (por defecto esfera)
		Ref<Model> m_PreviewModel;

		// Escena temporal para preview (contiene luces)
		Ref<Scene> m_PreviewScene;

		// Cámara para el preview
		EditorCamera m_Camera;

		// Iluminación
		Ref<Light> m_MainLight;
		glm::vec3 m_LightColor = glm::vec3(1.0f);
		float m_LightIntensity = 1.0f;

		// Fondo
		glm::vec4 m_BackgroundColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

		// Rotación
		bool m_AutoRotate = true;
		float m_RotationSpeed = 20.0f; // grados por segundo
		float m_CurrentRotation = 0.0f;

		// ========== HELPERS PRIVADOS ==========

		void InitializeFramebuffer();
		void InitializePreviewScene();
		void RenderInternal(Ref<MaterialAsset> material);
		
		// Copy framebuffer content to a standalone texture
		Ref<Texture2D> CopyFramebufferToTexture();

		// ? NEW: Disk cache helpers
		std::filesystem::path GetThumbnailCachePath() const;
		std::filesystem::path GetThumbnailPath(const std::filesystem::path& materialPath) const;
		bool IsThumbnailValid(const std::filesystem::path& thumbnailPath, const std::filesystem::path& materialPath) const;
		bool SaveThumbnailToDisk(const std::filesystem::path& thumbnailPath, Ref<Texture2D> thumbnail);
		Ref<Texture2D> LoadThumbnailFromDisk(const std::filesystem::path& thumbnailPath);
	};

}
