#pragma once

#include "Core/Core.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/MaterialAsset.h"
#include "Renderer/MaterialInstance.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/Model.h"
#include "Renderer/Light.h"
#include <glm/glm.hpp>

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

		// Obtener la textura renderizada
		uint32_t GetPreviewTextureID() const;
		Ref<Texture2D> GetPreviewTexture() const { return m_PreviewTexture; }

		// ========== THUMBNAIL GENERATION ==========

		// Generar thumbnail estático (no se actualiza automáticamente)
		Ref<Texture2D> GenerateThumbnail(Ref<MaterialAsset> material, uint32_t size = 256);

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
	};

}
