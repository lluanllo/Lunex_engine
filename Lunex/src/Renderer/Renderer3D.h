#pragma once

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/Model.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Lunex {
	// Forward declarations
	struct MeshComponent;
	struct MaterialComponent;
	struct TextureComponent;
	class Scene;

	class Renderer3D {
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const OrthographicCamera& camera);
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void BeginScene(const EditorCamera& camera);
		static void EndScene();

		static void UpdateLights(Scene* scene);

		static void DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID = -1);
		static void DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, int entityID = -1);
		static void DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID = -1);
		static void DrawModel(const glm::mat4& transform, const Ref<Model>& model, const glm::vec4& color = glm::vec4(1.0f), int entityID = -1);
		static void DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, int entityID = -1);
		static void DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID = -1);

		struct Statistics {
			uint32_t DrawCalls = 0;
			uint32_t MeshCount = 0;
			uint32_t TriangleCount = 0;
		};

		static void ResetStats();
		static Statistics GetStats();
	};

}