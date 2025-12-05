#pragma once

#include "Asset.h"
#include "Core/Core.h"
#include "Core/UUID.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <filesystem>

namespace Lunex {

	// Forward declarations
	class Scene;
	class Entity;

	// ============================================================================
	// PREFAB COMPONENT DATA
	// Serializable structure for storing component data
	// ============================================================================
	struct PrefabComponentData {
		std::string ComponentType;
		std::string SerializedData;  // YAML string of component data
	};

	// ============================================================================
	// PREFAB ENTITY DATA
	// Stores all data needed to recreate an entity
	// ============================================================================
	struct PrefabEntityData {
		UUID EntityID;
		std::string Tag;
		std::vector<PrefabComponentData> Components;
		
		// Hierarchy data (relative to prefab root)
		UUID LocalParentID = 0;  // Parent within the prefab (0 = root)
		std::vector<UUID> LocalChildIDs;
	};

	// ============================================================================
	// PREFAB METADATA
	// Lightweight info about the prefab
	// ============================================================================
	struct PrefabMetadata {
		std::string Name;
		std::string Description;
		uint32_t EntityCount = 0;
		std::filesystem::file_time_type LastModified;
		
		// Preview/thumbnail info (future)
		glm::vec3 BoundsMin = glm::vec3(0.0f);
		glm::vec3 BoundsMax = glm::vec3(0.0f);
	};

	// ============================================================================
	// PREFAB ASSET
	// Reusable entity template that can be instantiated in scenes
	// ============================================================================
	class Prefab : public Asset {
	public:
		Prefab();
		Prefab(const std::string& name);
		virtual ~Prefab() = default;

		// ========== ASSET INTERFACE ==========
		virtual AssetType GetType() const override { return AssetType::Prefab; }
		virtual bool SaveToFile(const std::filesystem::path& path) override;

		// ========== STATIC FACTORY METHODS ==========
		
		// Create a prefab from an entity (and optionally its children)
		static Ref<Prefab> CreateFromEntity(Entity entity, bool includeChildren = true);
		
		// Load a prefab from file
		static Ref<Prefab> LoadFromFile(const std::filesystem::path& path);

		// ========== PREFAB OPERATIONS ==========
		
		// Instantiate the prefab in a scene at given position
		// Returns the root entity of the instantiated prefab
		Entity Instantiate(const Ref<Scene>& scene, const glm::vec3& position = glm::vec3(0.0f));
		
		// Instantiate as child of another entity
		Entity InstantiateAsChild(const Ref<Scene>& scene, Entity parent, const glm::vec3& localPosition = glm::vec3(0.0f));

		// ========== METADATA ==========
		
		const PrefabMetadata& GetPrefabMetadata() const { return m_Metadata; }
		void SetDescription(const std::string& description) { m_Metadata.Description = description; MarkDirty(); }
		uint32_t GetEntityCount() const { return m_Metadata.EntityCount; }

		// ========== ENTITY DATA ACCESS ==========
		
		const std::vector<PrefabEntityData>& GetEntityData() const { return m_EntityData; }
		UUID GetRootEntityID() const { return m_RootEntityID; }

		// ========== VALIDATION ==========
		
		bool IsValid() const { return !m_EntityData.empty(); }
		bool HasHierarchy() const { return m_EntityData.size() > 1; }

	private:
		// Serialize entity and its components to PrefabEntityData
		void SerializeEntityToData(Entity entity, bool isRoot);
		
		// Recursively serialize entity hierarchy
		void SerializeEntityHierarchy(Entity entity, UUID parentLocalID);
		
		// Create entity from PrefabEntityData
		Entity DeserializeEntityFromData(const Ref<Scene>& scene, const PrefabEntityData& data, 
										std::unordered_map<UUID, Entity>& idMapping);
		
		// Apply transform offset to instantiated entity
		void ApplyTransformOffset(Entity entity, const glm::vec3& offset);
		
		// Helper to get reference scene for serialization
		Scene* GetEntityScene(Entity entity) const;

	private:
		PrefabMetadata m_Metadata;
		std::vector<PrefabEntityData> m_EntityData;
		UUID m_RootEntityID = 0;
		
		// Original root entity's transform (for offset calculation during instantiation)
		glm::vec3 m_OriginalRootPosition = glm::vec3(0.0f);
		glm::vec3 m_OriginalRootRotation = glm::vec3(0.0f);
		glm::vec3 m_OriginalRootScale = glm::vec3(1.0f);
	};

}
