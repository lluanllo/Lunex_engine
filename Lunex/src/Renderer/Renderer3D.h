#pragma once

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Resources/Mesh/Model.h"
#include "Scene/Camera/OrthographicCamera.h"
#include "Scene/Camera/EditorCamera.h"
#include "Renderer/EnvironmentMap.h"

#include <glm/glm.hpp>

namespace Lunex {
	// Forward declarations
	struct MeshComponent;
	struct MaterialComponent;
	struct TextureComponent;
	struct SkeletalMeshComponent;
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
		
		// IBL Environment binding
		static void BindEnvironment(const Ref<EnvironmentMap>& environment);
		static void UnbindEnvironment();

		static void DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, int entityID = -1);
		static void DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, int entityID = -1);
		static void DrawMesh(const glm::mat4& transform, MeshComponent& meshComponent, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID = -1);
		static void DrawModel(const glm::mat4& transform, const Ref<Model>& model, const glm::vec4& color = glm::vec4(1.0f), int entityID = -1);
		static void DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, int entityID = -1);
		static void DrawModel(const glm::mat4& transform, const Ref<Model>& model, MaterialComponent& materialComponent, TextureComponent& textureComponent, int entityID = -1);

		// Skinned mesh rendering (with GPU skinning support)
		static void DrawSkinnedMesh(const glm::mat4& transform, SkeletalMeshComponent& skeletalMesh, int entityID = -1);
		static void DrawSkinnedMesh(const glm::mat4& transform, SkeletalMeshComponent& skeletalMesh, MaterialComponent& materialComponent, int entityID = -1);

		struct Statistics {
			uint32_t DrawCalls = 0;
			uint32_t MeshCount = 0;
			uint32_t TriangleCount = 0;
		};

		static void ResetStats();
		static Statistics GetStats();
		
	private:
		// Internal helper for SkinnedModel rendering
		static void DrawSkinnedModelInternal(const glm::mat4& transform, SkeletalMeshComponent& skeletalMesh, MaterialComponent* materialComponent, int entityID);
	};

}
