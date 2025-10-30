#pragma once

#include "Core/Core.h"
#include "Renderer/Renderer3D/RendererPipeline3D.h"
#include "Renderer/Renderer3D/Mesh.h"
#include "Renderer/MaterialSystem/MaterialInstance.h"
#include "Renderer/CameraTypes/EditorCamera.h"
#include "Renderer/CameraTypes/OrthographicCamera.h"
#include "Renderer/Camera.h"

#include <glm/glm.hpp>

namespace Lunex {

	/**
	 * Renderer3D - API Estática para renderizado 3D
	 * Wrapper sobre RendererPipeline3D
	 */
	class Renderer3D {
	public:
		static void Init();
		static void Shutdown();

		// Scene control
		static void BeginScene(const EditorCamera& camera);
		static void BeginScene(const EditorCamera& camera, const glm::mat4& transform);
		static void BeginScene(const Camera& camera, const glm::mat4& transform); // ? Nueva sobrecarga para SceneCamera
		static void EndScene();

		// Submissions
		static void DrawMesh(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform, uint32_t entityID = 0);
		
		// Primitives
		static void DrawCube(const glm::vec3& position, const glm::vec3& scale, const Ref<Material>& material);
		static void DrawSphere(const glm::vec3& position, float radius, const Ref<Material>& material);

		// Stats
		static RendererPipeline3D::Statistics GetStats();
		static void ResetStats();

		// Access to pipeline (for advanced use)
		static Ref<RendererPipeline3D> GetPipeline() { return s_Pipeline; }

	private:
		static Ref<RendererPipeline3D> s_Pipeline;
	};

}
