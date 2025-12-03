#pragma once

#include "Core/Core.h"
#include "Core/JobSystem/JobSystem.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include <vector>
#include <string>
#include <variant>
#include <functional>

namespace Lunex {

	/// <summary>
	/// Component descriptor for batch entity creation.
	/// Uses std::variant to store different component types.
	/// </summary>
	struct ComponentDescriptor {
		enum class Type {
			Transform,
			Sprite,
			Mesh,
			Material,
			Light,
			Rigidbody2D,
			BoxCollider2D,
			Rigidbody3D,
			BoxCollider3D,
			// Add more as needed
		};

		Type ComponentType;

		// Component data (use variant for type-safe storage)
		std::variant<
			TransformComponent,
			SpriteRendererComponent,
			MeshComponent,
			MaterialComponent,
			LightComponent,
			Rigidbody2DComponent,
			BoxCollider2DComponent,
			Rigidbody3DComponent,
			BoxCollider3DComponent
		> Data;

		// Factory methods for convenience
		static ComponentDescriptor CreateTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale);
		static ComponentDescriptor CreateSprite(const glm::vec4& color);
		static ComponentDescriptor CreateMesh(ModelType type);
		static ComponentDescriptor CreateMaterial();
		static ComponentDescriptor CreateLight(LightType type, const glm::vec3& color, float intensity);
	};

	/// <summary>
	/// Entity descriptor for batch creation.
	/// Contains entity name and list of components to add.
	/// </summary>
	struct EntityDescriptor {
		std::string Name;
		std::vector<ComponentDescriptor> Components;

		EntityDescriptor() = default;

		explicit EntityDescriptor(const std::string& name)
			: Name(name) {}

		/// <summary>
		/// Add component to descriptor (builder pattern).
		/// </summary>
		EntityDescriptor& AddComponent(const ComponentDescriptor& component) {
			Components.push_back(component);
			return *this;
		}
	};

	/// <summary>
	/// Two-phase batch entity creation for safe multithreading.
	/// 
	/// Phase 1 (Parallel): Prepare CPU-side entity data
	/// Phase 2 (Main Thread): Actually create entities in ECS
	/// 
	/// This pattern allows expensive preparation work (e.g., procedural generation)
	/// to run in parallel, then atomically commits to ECS on main thread.
	/// 
	/// Usage Example:
	/// ```cpp
	/// std::vector<EntityDescriptor> entities;
	/// 
	/// // Create 1000 procedural entities
	/// for (int i = 0; i < 1000; ++i) {
	///     EntityDescriptor desc("Entity_" + std::to_string(i));
	///     desc.AddComponent(ComponentDescriptor::CreateTransform(
	///         glm::vec3(i, 0, 0), glm::vec3(0), glm::vec3(1)
	///     ));
	///     desc.AddComponent(ComponentDescriptor::CreateMesh(ModelType::Cube));
	///     entities.push_back(desc);
	/// }
	/// 
	/// // Create entities in parallel
	/// EntityBatchCreator::Get().CreateEntitiesBatch(
	///     activeScene,
	///     entities,
	///     []() { LNX_LOG_INFO("1000 entities created!"); }
	/// );
	/// ```
	/// </summary>
	class EntityBatchCreator {
	public:
		EntityBatchCreator() = default;
		~EntityBatchCreator() = default;

		// Singleton access
		static EntityBatchCreator& Get();

		/// <summary>
		/// Create multiple entities in parallel, then commit on main thread.
		/// </summary>
		/// <param name="scene">Target scene</param>
		/// <param name="descriptors">Entity descriptors to create</param>
		/// <param name="onComplete">Completion callback (called on main thread)</param>
		/// <param name="sceneVersion">Scene version for cancellation</param>
		void CreateEntitiesBatch(
			Scene* scene,
			const std::vector<EntityDescriptor>& descriptors,
			std::function<void()> onComplete = nullptr,
			uint64_t sceneVersion = 0
		);

		/// <summary>
		/// Create entities in parallel using generator function.
		/// Useful for procedural generation.
		/// </summary>
		/// <param name="scene">Target scene</param>
		/// <param name="count">Number of entities to create</param>
		/// <param name="generator">Function to generate entity descriptor per index</param>
		/// <param name="onComplete">Completion callback</param>
		/// <param name="sceneVersion">Scene version for cancellation</param>
		void CreateEntitiesProceduralBatch(
			Scene* scene,
			uint32_t count,
			std::function<EntityDescriptor(uint32_t)> generator,
			std::function<void()> onComplete = nullptr,
			uint64_t sceneVersion = 0
		);

	private:
		/// <summary>
		/// Phase 1: Prepare entity data (parallel on workers).
		/// </summary>
		/// <param name="descriptors">Entity descriptors</param>
		/// <returns>Prepared data ready for ECS insertion</returns>
		static std::vector<EntityDescriptor> PrepareEntities(const std::vector<EntityDescriptor>& descriptors);

		/// <summary>
		/// Phase 2: Commit entities to ECS (main thread).
		/// </summary>
		/// <param name="scene">Target scene</param>
		/// <param name="descriptors">Prepared entity descriptors</param>
		static void CommitEntities(Scene* scene, const std::vector<EntityDescriptor>& descriptors);

		// Singleton instance
		static EntityBatchCreator s_Instance;
	};

	// ========================================================================
	// USAGE EXAMPLES
	// ========================================================================

	// Example 1: Create Grid of Cubes
	// --------------------------------
	// void CreateCubeGrid(Scene* scene, uint32_t size) {
	//     EntityBatchCreator::Get().CreateEntitiesProceduralBatch(
	//         scene,
	//         size * size,
	//         [size](uint32_t index) {
	//             uint32_t x = index % size;
	//             uint32_t z = index / size;
	//             
	//             EntityDescriptor desc(fmt::format("Cube_{}_{}", x, z));
	//             desc.AddComponent(ComponentDescriptor::CreateTransform(
	//                 glm::vec3(x * 2.0f, 0, z * 2.0f),
	//                 glm::vec3(0),
	//                 glm::vec3(1)
	//             ));
	//             desc.AddComponent(ComponentDescriptor::CreateMesh(ModelType::Cube));
	//             desc.AddComponent(ComponentDescriptor::CreateMaterial());
	//             
	//             return desc;
	//         },
	//         []() { LNX_LOG_INFO("Grid created!"); }
	//     );
	// }

	// Example 2: Load Entities from File
	// -----------------------------------
	// void LoadEntitiesFromFile(Scene* scene, const std::string& path) {
	//     // Phase 1: Load file (IO thread)
	//     AssetLoadingPipeline::LoadRequest req;
	//     req.FilePath = path;
	//     req.Type = AssetType::Scene;
	//     req.OnComplete = [scene](Ref<Asset> asset) {
	//         auto* sceneData = static_cast<SceneData*>(asset.get());
	//         
	//         // Phase 2: Create entities (parallel + main thread)
	//         EntityBatchCreator::Get().CreateEntitiesBatch(
	//             scene,
	//             sceneData->Entities,
	//             []() { LNX_LOG_INFO("Scene loaded!"); }
	//         );
	//     };
	//     AssetLoadingPipeline::Get().LoadAssetAsync(req);
	// }

} // namespace Lunex
