#pragma once

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/Model.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/EditorCamera.h"
#include "Scene/Components.h"

#include <glm/glm.hpp>

namespace Lunex {

	class Renderer3D {
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const OrthographicCamera& camera);
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void BeginScene(const EditorCamera& camera);
		static void EndScene();

		static void DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID = -1);
		static void DrawModel(const glm::mat4& transform, const Ref<Model>& model, const glm::vec4& color = glm::vec4(1.0f), int entityID = -1);

		static void SetLightPosition(const glm::vec3& position);
		static void SetLightColor(const glm::vec3& color);

		struct Statistics {
			uint32_t DrawCalls = 0;
			uint32_t MeshCount = 0;
			uint32_t TriangleCount = 0;
		};

		static void ResetStats();
		static Statistics GetStats();

	private:
		struct SceneData {
			glm::mat4 ViewProjection;
			glm::vec3 CameraPosition;
			glm::vec3 LightPosition = { 2.0f, 4.0f, 2.0f };
			glm::vec3 LightColor = { 1.0f, 1.0f, 1.0f };
		};

		static SceneData s_SceneData;
		static Statistics s_Stats;
		static Ref<Shader> s_MeshShader;
	};

}
