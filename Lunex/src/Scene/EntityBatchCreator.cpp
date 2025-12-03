#include "stpch.h"
#include "EntityBatchCreator.h"
#include "Entity.h"
#include "Log/Log.h"

namespace Lunex {

	// Singleton instance
	EntityBatchCreator EntityBatchCreator::s_Instance;

	EntityBatchCreator& EntityBatchCreator::Get() {
		return s_Instance;
	}

	// ========================================================================
	// COMPONENT DESCRIPTOR FACTORIES
	// ========================================================================

	ComponentDescriptor ComponentDescriptor::CreateTransform(
		const glm::vec3& pos,
		const glm::vec3& rot,
		const glm::vec3& scale
	) {
		ComponentDescriptor desc;
		desc.ComponentType = Type::Transform;

		TransformComponent transform;
		transform.Translation = pos;
		transform.Rotation = rot;
		transform.Scale = scale;

		desc.Data = transform;
		return desc;
	}

	ComponentDescriptor ComponentDescriptor::CreateSprite(const glm::vec4& color) {
		ComponentDescriptor desc;
		desc.ComponentType = Type::Sprite;

		SpriteRendererComponent sprite;
		sprite.Color = color;

		desc.Data = sprite;
		return desc;
	}

	ComponentDescriptor ComponentDescriptor::CreateMesh(ModelType type) {
		ComponentDescriptor desc;
		desc.ComponentType = Type::Mesh;

		MeshComponent mesh;
		mesh.Type = type;
		mesh.CreatePrimitive(type);

		desc.Data = mesh;
		return desc;
	}

	ComponentDescriptor ComponentDescriptor::CreateMaterial() {
		ComponentDescriptor desc;
		desc.ComponentType = Type::Material;

		MaterialComponent material;
		desc.Data = material;
		return desc;
	}

	ComponentDescriptor ComponentDescriptor::CreateLight(
		LightType type,
		const glm::vec3& color,
		float intensity
	) {
		ComponentDescriptor desc;
		desc.ComponentType = Type::Light;

		LightComponent light;
		light.SetType(type);
		light.SetColor(color);
		light.SetIntensity(intensity);

		desc.Data = light;
		return desc;
	}

	// ========================================================================
	// BATCH CREATION
	// ========================================================================

	void EntityBatchCreator::CreateEntitiesBatch(
		Scene* scene,
		const std::vector<EntityDescriptor>& descriptors,
		std::function<void()> onComplete,
		uint64_t sceneVersion
	) {
		LNX_CORE_ASSERT(scene, "Scene cannot be null!");

		if (descriptors.empty()) {
			if (onComplete) onComplete();
			return;
		}

		LNX_LOG_INFO("Creating batch of {0} entities...", descriptors.size());

		// Create counter for job completion
		auto counter = JobSystem::Get().CreateCounter(1);

		// Phase 1: Prepare entities (parallel)
		// NOTE: For simple cases, preparation is just copying descriptors
		// For complex cases (procedural generation), this can be expensive
		JobSystem::Get().Schedule(
			[scene, descriptors, counter, onComplete, sceneVersion]() {
				auto prepared = PrepareEntities(descriptors);

				counter->Decrement();

				// Phase 2: Commit to ECS (main thread)
				// Wrap scene pointer and descriptors in a struct for ownership transfer
				struct CommitData {
					Scene* ScenePtr;
					std::vector<EntityDescriptor> Descriptors;
					std::function<void()> OnComplete;
				};

				auto commitData = std::make_unique<CommitData>();
				commitData->ScenePtr = scene;
				commitData->Descriptors = std::move(prepared);
				commitData->OnComplete = onComplete;

				auto cmd = Command::CreateWithOwnership<CommitData>(
					sceneVersion,
					std::move(commitData),
					[](MainThreadContext& ctx, CommitData* data) {
						CommitEntities(data->ScenePtr, data->Descriptors);

						if (data->OnComplete) {
							data->OnComplete();
						}
					}
				);

				JobSystem::Get().PushMainThreadCommand(std::move(cmd));
			},
			nullptr,
			nullptr,
			JobPriority::Normal,
			sceneVersion
		);
	}

	void EntityBatchCreator::CreateEntitiesProceduralBatch(
		Scene* scene,
		uint32_t count,
		std::function<EntityDescriptor(uint32_t)> generator,
		std::function<void()> onComplete,
		uint64_t sceneVersion
	) {
		LNX_CORE_ASSERT(scene, "Scene cannot be null!");
		LNX_CORE_ASSERT(generator, "Generator function cannot be null!");

		if (count == 0) {
			if (onComplete) onComplete();
			return;
		}

		LNX_LOG_INFO("Creating procedural batch of {0} entities...", count);

		// Generate descriptors in parallel
		auto counter = JobSystem::Get().ParallelFor(
			0,
			count,
			[generator, count, scene, onComplete, sceneVersion](uint32_t index) {
				// This lambda runs in parallel for each index
				static thread_local std::vector<EntityDescriptor> threadLocalBuffer;

				// Generate entity descriptor
				EntityDescriptor desc = generator(index);
				threadLocalBuffer.push_back(std::move(desc));

				// When last index completes, merge and commit
				// NOTE: This is a simplification. In production, use proper reduction.
				if (index == count - 1) {
					// Commit to ECS (main thread)
					struct CommitData {
						Scene* ScenePtr;
						std::vector<EntityDescriptor> Descriptors;
						std::function<void()> OnComplete;
					};

					auto commitData = std::make_unique<CommitData>();
					commitData->ScenePtr = scene;
					commitData->Descriptors = std::move(threadLocalBuffer);
					commitData->OnComplete = onComplete;

					auto cmd = Command::CreateWithOwnership<CommitData>(
						sceneVersion,
						std::move(commitData),
						[](MainThreadContext& ctx, CommitData* data) {
							CommitEntities(data->ScenePtr, data->Descriptors);

							if (data->OnComplete) {
								data->OnComplete();
							}
						}
					);

					JobSystem::Get().PushMainThreadCommand(std::move(cmd));
					threadLocalBuffer.clear();
				}
			},
			0, // Auto grain size
			JobPriority::Normal,
			sceneVersion
		);
	}

	// ========================================================================
	// INTERNAL PHASES
	// ========================================================================

	std::vector<EntityDescriptor> EntityBatchCreator::PrepareEntities(
		const std::vector<EntityDescriptor>& descriptors
	) {
		// For simple cases, just return copy
		// For complex cases, this could do expensive preprocessing
		return descriptors;
	}

	void EntityBatchCreator::CommitEntities(
		Scene* scene,
		const std::vector<EntityDescriptor>& descriptors
	) {
		LNX_LOG_INFO("Committing {0} entities to ECS...", descriptors.size());

		for (const auto& desc : descriptors) {
			// Create entity
			Entity entity = scene->CreateEntity(desc.Name);

			// Add components
			for (const auto& compDesc : desc.Components) {
				switch (compDesc.ComponentType) {
				case ComponentDescriptor::Type::Transform: {
					if (!entity.HasComponent<TransformComponent>()) {
						// Transform is added by default, so replace it
						auto& transform = entity.GetComponent<TransformComponent>();
						transform = std::get<TransformComponent>(compDesc.Data);
					}
					break;
				}

				case ComponentDescriptor::Type::Sprite: {
					entity.AddComponent<SpriteRendererComponent>(
						std::get<SpriteRendererComponent>(compDesc.Data)
					);
					break;
				}

				case ComponentDescriptor::Type::Mesh: {
					entity.AddComponent<MeshComponent>(
						std::get<MeshComponent>(compDesc.Data)
					);
					break;
				}

				case ComponentDescriptor::Type::Material: {
					entity.AddComponent<MaterialComponent>(
						std::get<MaterialComponent>(compDesc.Data)
					);
					break;
				}

				case ComponentDescriptor::Type::Light: {
					entity.AddComponent<LightComponent>(
						std::get<LightComponent>(compDesc.Data)
					);
					break;
				}

				case ComponentDescriptor::Type::Rigidbody2D: {
					entity.AddComponent<Rigidbody2DComponent>(
						std::get<Rigidbody2DComponent>(compDesc.Data)
					);
					break;
				}

				case ComponentDescriptor::Type::BoxCollider2D: {
					entity.AddComponent<BoxCollider2DComponent>(
						std::get<BoxCollider2DComponent>(compDesc.Data)
					);
					break;
				}

				case ComponentDescriptor::Type::Rigidbody3D: {
					entity.AddComponent<Rigidbody3DComponent>(
						std::get<Rigidbody3DComponent>(compDesc.Data)
					);
					break;
				}

				case ComponentDescriptor::Type::BoxCollider3D: {
					entity.AddComponent<BoxCollider3DComponent>(
						std::get<BoxCollider3DComponent>(compDesc.Data)
					);
					break;
				}

				default:
					LNX_LOG_WARN("Unknown component type in EntityDescriptor");
					break;
				}
			}
		}

		LNX_LOG_INFO("Committed {0} entities successfully", descriptors.size());
	}

} // namespace Lunex
