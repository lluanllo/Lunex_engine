#include "stpch.h"
#include "Prefab.h"

#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Log/Log.h"

#include <fstream>
#include <sstream>
#include <yaml-cpp/yaml.h>

namespace Lunex {

	// ============================================================================
	// YAML SERIALIZATION HELPERS (copied from SceneSerializer for consistency)
	// ============================================================================
	
	namespace YAML_Prefab {
		YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v) {
			out << YAML::Flow;
			out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
			return out;
		}
		
		YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v) {
			out << YAML::Flow;
			out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
			return out;
		}
		
		YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v) {
			out << YAML::Flow;
			out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
			return out;
		}
	}

	// ============================================================================
	// PREFAB IMPLEMENTATION
	// ============================================================================

	Prefab::Prefab() 
		: Asset("New Prefab") {
		m_Metadata.Name = "New Prefab";
	}

	Prefab::Prefab(const std::string& name)
		: Asset(name) {
		m_Metadata.Name = name;
	}

	// ============================================================================
	// STATIC FACTORY METHODS
	// ============================================================================

	Ref<Prefab> Prefab::CreateFromEntity(Entity entity, bool includeChildren) {
		if (!entity) {
			LNX_LOG_ERROR("Prefab::CreateFromEntity - Invalid entity");
			return nullptr;
		}

		Ref<Prefab> prefab = CreateRef<Prefab>();
		
		// Get entity name for prefab name
		std::string entityName = "Prefab";
		if (entity.HasComponent<TagComponent>()) {
			entityName = entity.GetComponent<TagComponent>().Tag;
		}
		prefab->SetName(entityName);
		prefab->m_Metadata.Name = entityName;
		
		// Store root entity ID
		prefab->m_RootEntityID = entity.GetUUID();
		
		// Store original transform for offset calculation
		if (entity.HasComponent<TransformComponent>()) {
			auto& tc = entity.GetComponent<TransformComponent>();
			prefab->m_OriginalRootPosition = tc.Translation;
			prefab->m_OriginalRootRotation = tc.Rotation;
			prefab->m_OriginalRootScale = tc.Scale;
		}
		
		// Serialize entity hierarchy
		if (includeChildren && entity.HasComponent<RelationshipComponent>()) {
			prefab->SerializeEntityHierarchy(entity, 0);
		} else {
			prefab->SerializeEntityToData(entity, true);
		}
		
		prefab->m_Metadata.EntityCount = static_cast<uint32_t>(prefab->m_EntityData.size());
		
		LNX_LOG_INFO("Prefab::CreateFromEntity - Created prefab '{0}' with {1} entities", 
			entityName, prefab->m_Metadata.EntityCount);
		
		return prefab;
	}

	Ref<Prefab> Prefab::LoadFromFile(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("Prefab::LoadFromFile - File not found: {0}", path.string());
			return nullptr;
		}

		YAML::Node data;
		try {
			data = YAML::LoadFile(path.string());
		}
		catch (const YAML::ParserException& e) {
			LNX_LOG_ERROR("Prefab::LoadFromFile - YAML parse error: {0}", e.what());
			return nullptr;
		}

		if (!data["Prefab"]) {
			LNX_LOG_ERROR("Prefab::LoadFromFile - Invalid prefab file (no 'Prefab' node): {0}", path.string());
			return nullptr;
		}

		Ref<Prefab> prefab = CreateRef<Prefab>();
		prefab->SetPath(path);
		
		// Load metadata
		auto prefabNode = data["Prefab"];
		if (prefabNode["Name"]) {
			prefab->SetName(prefabNode["Name"].as<std::string>());
			prefab->m_Metadata.Name = prefab->GetName();
		}
		if (prefabNode["Description"]) {
			prefab->m_Metadata.Description = prefabNode["Description"].as<std::string>();
		}
		if (prefabNode["RootEntityID"]) {
			prefab->m_RootEntityID = UUID(prefabNode["RootEntityID"].as<uint64_t>());
		}
		
		// Load original root transform
		if (prefabNode["OriginalTransform"]) {
			auto transform = prefabNode["OriginalTransform"];
			if (transform["Position"]) {
				auto pos = transform["Position"];
				prefab->m_OriginalRootPosition = glm::vec3(
					pos[0].as<float>(), pos[1].as<float>(), pos[2].as<float>()
				);
			}
			if (transform["Rotation"]) {
				auto rot = transform["Rotation"];
				prefab->m_OriginalRootRotation = glm::vec3(
					rot[0].as<float>(), rot[1].as<float>(), rot[2].as<float>()
				);
			}
			if (transform["Scale"]) {
				auto scl = transform["Scale"];
				prefab->m_OriginalRootScale = glm::vec3(
					scl[0].as<float>(), scl[1].as<float>(), scl[2].as<float>()
				);
			}
		}
		
		// Load entity data
		auto entities = data["Entities"];
		if (entities && entities.IsSequence()) {
			for (auto entityNode : entities) {
				PrefabEntityData entityData;
				
				if (entityNode["EntityID"]) {
					entityData.EntityID = UUID(entityNode["EntityID"].as<uint64_t>());
				}
				if (entityNode["Tag"]) {
					entityData.Tag = entityNode["Tag"].as<std::string>();
				}
				if (entityNode["LocalParentID"]) {
					entityData.LocalParentID = UUID(entityNode["LocalParentID"].as<uint64_t>());
				}
				
				// Load local child IDs
				if (entityNode["LocalChildIDs"] && entityNode["LocalChildIDs"].IsSequence()) {
					for (auto childIDNode : entityNode["LocalChildIDs"]) {
						entityData.LocalChildIDs.push_back(UUID(childIDNode.as<uint64_t>()));
					}
				}
				
				// Load components
				if (entityNode["Components"] && entityNode["Components"].IsSequence()) {
					for (auto compNode : entityNode["Components"]) {
						PrefabComponentData compData;
						if (compNode["Type"]) {
							compData.ComponentType = compNode["Type"].as<std::string>();
						}
						if (compNode["Data"]) {
							compData.SerializedData = compNode["Data"].as<std::string>();
						}
						entityData.Components.push_back(compData);
					}
				}
				
				prefab->m_EntityData.push_back(entityData);
			}
		}
		
		prefab->m_Metadata.EntityCount = static_cast<uint32_t>(prefab->m_EntityData.size());
		prefab->ClearDirty();
		
		LNX_LOG_INFO("Prefab::LoadFromFile - Loaded prefab '{0}' with {1} entities from {2}", 
			prefab->GetName(), prefab->m_Metadata.EntityCount, path.string());
		
		return prefab;
	}

	// ============================================================================
	// SAVE TO FILE
	// ============================================================================

	bool Prefab::SaveToFile(const std::filesystem::path& path) {
		YAML::Emitter out;
		out << YAML::BeginMap;
		
		// Prefab metadata
		out << YAML::Key << "Prefab" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << m_Metadata.Name;
		out << YAML::Key << "Description" << YAML::Value << m_Metadata.Description;
		out << YAML::Key << "RootEntityID" << YAML::Value << (uint64_t)m_RootEntityID;
		out << YAML::Key << "UUID" << YAML::Value << (uint64_t)m_ID;
		
		// Original root transform
		out << YAML::Key << "OriginalTransform" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Position" << YAML::Value;
		YAML_Prefab::operator<<(out, m_OriginalRootPosition);
		out << YAML::Key << "Rotation" << YAML::Value;
		YAML_Prefab::operator<<(out, m_OriginalRootRotation);
		out << YAML::Key << "Scale" << YAML::Value;
		YAML_Prefab::operator<<(out, m_OriginalRootScale);
		out << YAML::EndMap; // OriginalTransform
		
		out << YAML::EndMap; // Prefab
		
		// Entities
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		
		for (const auto& entityData : m_EntityData) {
			out << YAML::BeginMap;
			
			out << YAML::Key << "EntityID" << YAML::Value << (uint64_t)entityData.EntityID;
			out << YAML::Key << "Tag" << YAML::Value << entityData.Tag;
			out << YAML::Key << "LocalParentID" << YAML::Value << (uint64_t)entityData.LocalParentID;
			
			// Local children IDs
			out << YAML::Key << "LocalChildIDs" << YAML::Value << YAML::BeginSeq;
			for (UUID childID : entityData.LocalChildIDs) {
				out << (uint64_t)childID;
			}
			out << YAML::EndSeq;
			
			// Components
			out << YAML::Key << "Components" << YAML::Value << YAML::BeginSeq;
			for (const auto& compData : entityData.Components) {
				out << YAML::BeginMap;
				out << YAML::Key << "Type" << YAML::Value << compData.ComponentType;
				out << YAML::Key << "Data" << YAML::Value << compData.SerializedData;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq; // Components
			
			out << YAML::EndMap; // Entity
		}
		
		out << YAML::EndSeq; // Entities
		out << YAML::EndMap; // Root
		
		// Write to file
		std::ofstream fout(path);
		if (!fout) {
			LNX_LOG_ERROR("Prefab::SaveToFile - Failed to open file for writing: {0}", path.string());
			return false;
		}
		
		fout << out.c_str();
		fout.close();
		
		SetPath(path);
		ClearDirty();
		
		LNX_LOG_INFO("Prefab::SaveToFile - Saved prefab '{0}' to {1}", m_Metadata.Name, path.string());
		return true;
	}

	// ============================================================================
	// INSTANTIATION
	// ============================================================================

	Entity Prefab::Instantiate(const Ref<Scene>& scene, const glm::vec3& position) {
		if (!scene || m_EntityData.empty()) {
			LNX_LOG_ERROR("Prefab::Instantiate - Invalid scene or empty prefab");
			return {};
		}

		// Map from original prefab entity IDs to new scene entity IDs
		std::unordered_map<UUID, Entity> idMapping;
		
		// Create all entities first
		for (const auto& entityData : m_EntityData) {
			Entity newEntity = DeserializeEntityFromData(scene, entityData, idMapping);
			idMapping[entityData.EntityID] = newEntity;
		}
		
		// Rebuild hierarchy relationships with new IDs
		for (const auto& entityData : m_EntityData) {
			if (idMapping.find(entityData.EntityID) == idMapping.end())
				continue;
				
			Entity entity = idMapping[entityData.EntityID];
			
			if (entity.HasComponent<RelationshipComponent>()) {
				auto& rel = entity.GetComponent<RelationshipComponent>();
				
				// Update parent reference
				if (entityData.LocalParentID != 0) {
					if (idMapping.find(entityData.LocalParentID) != idMapping.end()) {
						rel.ParentID = idMapping[entityData.LocalParentID].GetUUID();
					}
				}
				
				// Update children references
				rel.ChildrenIDs.clear();
				for (UUID oldChildID : entityData.LocalChildIDs) {
					if (idMapping.find(oldChildID) != idMapping.end()) {
						rel.ChildrenIDs.push_back(idMapping[oldChildID].GetUUID());
					}
				}
			}
		}
		
		// Find and return root entity
		Entity rootEntity;
		if (idMapping.find(m_RootEntityID) != idMapping.end()) {
			rootEntity = idMapping[m_RootEntityID];
			
			// Apply position offset to root
			ApplyTransformOffset(rootEntity, position);
		}
		
		LNX_LOG_INFO("Prefab::Instantiate - Instantiated prefab '{0}' at ({1}, {2}, {3})", 
			m_Metadata.Name, position.x, position.y, position.z);
		
		return rootEntity;
	}

	Entity Prefab::InstantiateAsChild(const Ref<Scene>& scene, Entity parent, const glm::vec3& localPosition) {
		Entity rootEntity = Instantiate(scene, localPosition);
		
		if (rootEntity && parent) {
			// Set parent relationship
			if (!rootEntity.HasComponent<RelationshipComponent>()) {
				rootEntity.AddComponent<RelationshipComponent>();
			}
			
			auto& rootRel = rootEntity.GetComponent<RelationshipComponent>();
			rootRel.ParentID = parent.GetUUID();
			
			// Add to parent's children list
			if (!parent.HasComponent<RelationshipComponent>()) {
				parent.AddComponent<RelationshipComponent>();
			}
			
			auto& parentRel = parent.GetComponent<RelationshipComponent>();
			parentRel.AddChild(rootEntity.GetUUID());
		}
		
		return rootEntity;
	}

	// ============================================================================
	// SERIALIZATION HELPERS
	// ============================================================================

	void Prefab::SerializeEntityToData(Entity entity, bool isRoot) {
		PrefabEntityData data;
		data.EntityID = entity.GetUUID();
		
		if (entity.HasComponent<TagComponent>()) {
			data.Tag = entity.GetComponent<TagComponent>().Tag;
		}
		
		// Serialize TransformComponent
		if (entity.HasComponent<TransformComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "TransformComponent";
			
			auto& tc = entity.GetComponent<TransformComponent>();
			std::ostringstream oss;
			oss << tc.Translation.x << "," << tc.Translation.y << "," << tc.Translation.z << ";";
			oss << tc.Rotation.x << "," << tc.Rotation.y << "," << tc.Rotation.z << ";";
			oss << tc.Scale.x << "," << tc.Scale.y << "," << tc.Scale.z;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize CameraComponent
		if (entity.HasComponent<CameraComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "CameraComponent";
			
			auto& cc = entity.GetComponent<CameraComponent>();
			std::ostringstream oss;
			oss << (int)cc.Camera.GetProjectionType() << ";";
			oss << cc.Camera.GetPerspectiveVerticalFOV() << ";";
			oss << cc.Camera.GetPerspectiveNearClip() << ";";
			oss << cc.Camera.GetPerspectiveFarClip() << ";";
			oss << cc.Camera.GetOrthographicSize() << ";";
			oss << cc.Camera.GetOrthographicNearClip() << ";";
			oss << cc.Camera.GetOrthographicFarClip() << ";";
			oss << (cc.Primary ? "1" : "0") << ";";
			oss << (cc.FixedAspectRatio ? "1" : "0");
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize SpriteRendererComponent
		if (entity.HasComponent<SpriteRendererComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "SpriteRendererComponent";
			
			auto& src = entity.GetComponent<SpriteRendererComponent>();
			std::ostringstream oss;
			oss << src.Color.r << "," << src.Color.g << "," << src.Color.b << "," << src.Color.a << ";";
			oss << (src.Texture ? src.Texture->GetPath() : "") << ";";
			oss << src.TilingFactor;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize CircleRendererComponent
		if (entity.HasComponent<CircleRendererComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "CircleRendererComponent";
			
			auto& crc = entity.GetComponent<CircleRendererComponent>();
			std::ostringstream oss;
			oss << crc.Color.r << "," << crc.Color.g << "," << crc.Color.b << "," << crc.Color.a << ";";
			oss << crc.Thickness << ";";
			oss << crc.Fade;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize MeshComponent
		if (entity.HasComponent<MeshComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "MeshComponent";
			
			auto& mc = entity.GetComponent<MeshComponent>();
			std::ostringstream oss;
			oss << (int)mc.Type << ";";
			oss << mc.Color.r << "," << mc.Color.g << "," << mc.Color.b << "," << mc.Color.a << ";";
			oss << (uint64_t)mc.MeshAssetID << ";";
			oss << mc.MeshAssetPath << ";";
			oss << mc.FilePath;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize MaterialComponent
		if (entity.HasComponent<MaterialComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "MaterialComponent";
			
			auto& mat = entity.GetComponent<MaterialComponent>();
			std::ostringstream oss;
			oss << (uint64_t)mat.GetAssetID() << ";";
			oss << mat.GetAssetPath() << ";";
			oss << (mat.HasLocalOverrides() ? "1" : "0") << ";";
			auto albedo = mat.GetAlbedo();
			oss << albedo.r << "," << albedo.g << "," << albedo.b << "," << albedo.a << ";";
			oss << mat.GetMetallic() << ";";
			oss << mat.GetRoughness() << ";";
			oss << mat.GetSpecular() << ";";
			auto emission = mat.GetEmissionColor();
			oss << emission.r << "," << emission.g << "," << emission.b << ";";
			oss << mat.GetEmissionIntensity();
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize LightComponent
		if (entity.HasComponent<LightComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "LightComponent";
			
			auto& light = entity.GetComponent<LightComponent>();
			std::ostringstream oss;
			oss << (int)light.GetType() << ";";
			oss << light.GetColor().r << "," << light.GetColor().g << "," << light.GetColor().b << ";";
			oss << light.GetIntensity() << ";";
			oss << light.GetRange() << ";";
			auto atten = light.GetAttenuation();
			oss << atten.x << "," << atten.y << "," << atten.z << ";";
			oss << light.GetInnerConeAngle() << ";";
			oss << light.GetOuterConeAngle() << ";";
			oss << (light.GetCastShadows() ? "1" : "0");
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize Rigidbody2DComponent
		if (entity.HasComponent<Rigidbody2DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "Rigidbody2DComponent";
			
			auto& rb = entity.GetComponent<Rigidbody2DComponent>();
			std::ostringstream oss;
			oss << (int)rb.Type << ";";
			oss << (rb.FixedRotation ? "1" : "0");
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize BoxCollider2DComponent
		if (entity.HasComponent<BoxCollider2DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "BoxCollider2DComponent";
			
			auto& bc = entity.GetComponent<BoxCollider2DComponent>();
			std::ostringstream oss;
			oss << bc.Offset.x << "," << bc.Offset.y << ";";
			oss << bc.Size.x << "," << bc.Size.y << ";";
			oss << bc.Density << ";";
			oss << bc.Friction << ";";
			oss << bc.Restitution << ";";
			oss << bc.RestitutionThreshold;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize CircleCollider2DComponent
		if (entity.HasComponent<CircleCollider2DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "CircleCollider2DComponent";
			
			auto& cc = entity.GetComponent<CircleCollider2DComponent>();
			std::ostringstream oss;
			oss << cc.Offset.x << "," << cc.Offset.y << ";";
			oss << cc.Radius << ";";
			oss << cc.Density << ";";
			oss << cc.Friction << ";";
			oss << cc.Restitution << ";";
			oss << cc.RestitutionThreshold;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize Rigidbody3DComponent
		if (entity.HasComponent<Rigidbody3DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "Rigidbody3DComponent";
			
			auto& rb = entity.GetComponent<Rigidbody3DComponent>();
			std::ostringstream oss;
			oss << (int)rb.Type << ";";
			oss << rb.Mass << ";";
			oss << rb.Friction << ";";
			oss << rb.Restitution << ";";
			oss << rb.LinearDamping << ";";
			oss << rb.AngularDamping << ";";
			oss << rb.LinearFactor.x << "," << rb.LinearFactor.y << "," << rb.LinearFactor.z << ";";
			oss << rb.AngularFactor.x << "," << rb.AngularFactor.y << "," << rb.AngularFactor.z << ";";
			oss << (rb.UseCCD ? "1" : "0") << ";";
			oss << rb.CcdMotionThreshold << ";";
			oss << rb.CcdSweptSphereRadius << ";";
			oss << (rb.IsTrigger ? "1" : "0");
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize BoxCollider3DComponent
		if (entity.HasComponent<BoxCollider3DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "BoxCollider3DComponent";
			
			auto& bc = entity.GetComponent<BoxCollider3DComponent>();
			std::ostringstream oss;
			oss << bc.HalfExtents.x << "," << bc.HalfExtents.y << "," << bc.HalfExtents.z << ";";
			oss << bc.Offset.x << "," << bc.Offset.y << "," << bc.Offset.z;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize SphereCollider3DComponent
		if (entity.HasComponent<SphereCollider3DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "SphereCollider3DComponent";
			
			auto& sc = entity.GetComponent<SphereCollider3DComponent>();
			std::ostringstream oss;
			oss << sc.Radius << ";";
			oss << sc.Offset.x << "," << sc.Offset.y << "," << sc.Offset.z;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize CapsuleCollider3DComponent
		if (entity.HasComponent<CapsuleCollider3DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "CapsuleCollider3DComponent";
			
			auto& cc = entity.GetComponent<CapsuleCollider3DComponent>();
			std::ostringstream oss;
			oss << cc.Radius << ";";
			oss << cc.Height << ";";
			oss << cc.Offset.x << "," << cc.Offset.y << "," << cc.Offset.z;
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize MeshCollider3DComponent
		if (entity.HasComponent<MeshCollider3DComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "MeshCollider3DComponent";
			
			auto& mc = entity.GetComponent<MeshCollider3DComponent>();
			std::ostringstream oss;
			oss << (int)mc.Type << ";";
			oss << (mc.UseEntityMesh ? "1" : "0");
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		// Serialize ScriptComponent
		if (entity.HasComponent<ScriptComponent>()) {
			PrefabComponentData compData;
			compData.ComponentType = "ScriptComponent";
			
			auto& script = entity.GetComponent<ScriptComponent>();
			std::ostringstream oss;
			oss << (script.AutoCompile ? "1" : "0") << ";";
			for (size_t i = 0; i < script.GetScriptCount(); i++) {
				if (i > 0) oss << "|";
				oss << script.GetScriptPath(i);
			}
			compData.SerializedData = oss.str();
			
			data.Components.push_back(compData);
		}
		
		m_EntityData.push_back(data);
	}

	void Prefab::SerializeEntityHierarchy(Entity entity, UUID parentLocalID) {
		// Serialize this entity
		SerializeEntityToData(entity, parentLocalID == 0);
		
		// Update local parent ID
		m_EntityData.back().LocalParentID = parentLocalID;
		
		// Recursively serialize children
		if (entity.HasComponent<RelationshipComponent>()) {
			auto& rel = entity.GetComponent<RelationshipComponent>();
			
			for (UUID childID : rel.ChildrenIDs) {
				// Get child entity from scene
				Scene* scene = GetEntityScene(entity);
				if (scene) {
					Entity childEntity = scene->GetEntityByUUID(childID);
					if (childEntity) {
						m_EntityData.back().LocalChildIDs.push_back(childID);
						SerializeEntityHierarchy(childEntity, entity.GetUUID());
					}
				}
			}
		}
	}

	Scene* Prefab::GetEntityScene(Entity entity) const {
		return entity.GetScene();
	}

	Entity Prefab::DeserializeEntityFromData(const Ref<Scene>& scene, const PrefabEntityData& data,
											std::unordered_map<UUID, Entity>& idMapping) {
		// Create new entity with new UUID
		Entity entity = scene->CreateEntity(data.Tag);
		
		// Deserialize components
		for (const auto& compData : data.Components) {
			if (compData.ComponentType == "TransformComponent") {
				auto& tc = entity.GetComponent<TransformComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				// Translation
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &tc.Translation.x, &tc.Translation.y, &tc.Translation.z);
				
				// Rotation
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &tc.Rotation.x, &tc.Rotation.y, &tc.Rotation.z);
				
				// Scale
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &tc.Scale.x, &tc.Scale.y, &tc.Scale.z);
			}
			else if (compData.ComponentType == "CameraComponent") {
				auto& cc = entity.AddComponent<CameraComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				int projType;
				float perspFov, perspNear, perspFar, orthoSize, orthoNear, orthoFar;
				int primary, fixedAspect;
				
				std::getline(iss, token, ';'); projType = std::stoi(token);
				std::getline(iss, token, ';'); perspFov = std::stof(token);
				std::getline(iss, token, ';'); perspNear = std::stof(token);
				std::getline(iss, token, ';'); perspFar = std::stof(token);
				std::getline(iss, token, ';'); orthoSize = std::stof(token);
				std::getline(iss, token, ';'); orthoNear = std::stof(token);
				std::getline(iss, token, ';'); orthoFar = std::stof(token);
				std::getline(iss, token, ';'); primary = std::stoi(token);
				std::getline(iss, token, ';'); fixedAspect = std::stoi(token);
				
				cc.Camera.SetProjectionType((SceneCamera::ProjectionType)projType);
				cc.Camera.SetPerspectiveVerticalFOV(perspFov);
				cc.Camera.SetPerspectiveNearClip(perspNear);
				cc.Camera.SetPerspectiveFarClip(perspFar);
				cc.Camera.SetOrthographicSize(orthoSize);
				cc.Camera.SetOrthographicNearClip(orthoNear);
				cc.Camera.SetOrthographicFarClip(orthoFar);
				cc.Primary = primary != 0;
				cc.FixedAspectRatio = fixedAspect != 0;
			}
			else if (compData.ComponentType == "SpriteRendererComponent") {
				auto& src = entity.AddComponent<SpriteRendererComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				// Color
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f,%f", &src.Color.r, &src.Color.g, &src.Color.b, &src.Color.a);
				
				// Texture path
				std::getline(iss, token, ';');
				if (!token.empty()) {
					src.Texture = Texture2D::Create(token);
				}
				
				// Tiling factor
				std::getline(iss, token, ';');
				if (!token.empty()) {
					src.TilingFactor = std::stof(token);
				}
			}
			else if (compData.ComponentType == "CircleRendererComponent") {
				auto& crc = entity.AddComponent<CircleRendererComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				// Color
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f,%f", &crc.Color.r, &crc.Color.g, &crc.Color.b, &crc.Color.a);
				
				std::getline(iss, token, ';');
				crc.Thickness = std::stof(token);
				
				std::getline(iss, token, ';');
				crc.Fade = std::stof(token);
			}
			else if (compData.ComponentType == "MeshComponent") {
				auto& mc = entity.AddComponent<MeshComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				// Type
				std::getline(iss, token, ';');
				mc.Type = (ModelType)std::stoi(token);
				
				// Color
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f,%f", &mc.Color.r, &mc.Color.g, &mc.Color.b, &mc.Color.a);
				
				// MeshAssetID
				std::getline(iss, token, ';');
				uint64_t meshAssetID = 0;
				if (!token.empty()) {
					meshAssetID = std::stoull(token);
				}
				
				// MeshAssetPath (relative to assets folder)
				std::getline(iss, token, ';');
				std::string meshAssetPath = token;
				
				// FilePath (legacy)
				std::getline(iss, token, ';');
				std::string filePath = token;
				
				// Load mesh - prioritize MeshAsset over legacy FilePath
				if (!meshAssetPath.empty()) {
					// Resolve relative path to absolute path
					// The path is relative to the "assets" folder
					std::filesystem::path fullPath = std::filesystem::path("assets") / meshAssetPath;
					
					if (std::filesystem::exists(fullPath)) {
						mc.SetMeshAsset(fullPath);
						LNX_LOG_TRACE("Prefab: Loaded MeshAsset from {0}", fullPath.string());
					} else {
						LNX_LOG_WARN("Prefab: MeshAsset file not found: {0}", fullPath.string());
						// Fallback to primitive
						mc.CreatePrimitive(mc.Type != ModelType::FromFile ? mc.Type : ModelType::Cube);
					}
				} else if (!filePath.empty() && mc.Type == ModelType::FromFile) {
					// Legacy: direct file path
					if (std::filesystem::exists(filePath)) {
						mc.LoadFromFile(filePath);
					} else {
						LNX_LOG_WARN("Prefab: Model file not found: {0}", filePath);
						mc.CreatePrimitive(ModelType::Cube);
					}
				} else if (mc.Type != ModelType::FromFile) {
					mc.CreatePrimitive(mc.Type);
				} else {
					// No mesh source, default to cube
					mc.CreatePrimitive(ModelType::Cube);
				}
			}
			else if (compData.ComponentType == "MaterialComponent") {
				// MeshComponent already adds MaterialComponent, so get it
				MaterialComponent* mat = nullptr;
				if (entity.HasComponent<MaterialComponent>()) {
					mat = &entity.GetComponent<MaterialComponent>();
				} else {
					mat = &entity.AddComponent<MaterialComponent>();
				}
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				// AssetID
				std::getline(iss, token, ';');
				uint64_t assetID = std::stoull(token);
				
				// AssetPath
				std::getline(iss, token, ';');
				std::string assetPath = token;
				
				// HasLocalOverrides
				std::getline(iss, token, ';');
				bool hasOverrides = token == "1";
				
				// Load material if path exists
				if (!assetPath.empty()) {
					mat->SetMaterialAsset(assetPath);
				}
				
				if (hasOverrides) {
					// Albedo
					std::getline(iss, token, ';');
					glm::vec4 albedo;
					sscanf(token.c_str(), "%f,%f,%f,%f", &albedo.r, &albedo.g, &albedo.b, &albedo.a);
					mat->SetAlbedo(albedo, true);
					
					std::getline(iss, token, ';');
					mat->SetMetallic(std::stof(token), true);
					
					std::getline(iss, token, ';');
					mat->SetRoughness(std::stof(token), true);
					
					std::getline(iss, token, ';');
					mat->SetSpecular(std::stof(token), true);
					
					std::getline(iss, token, ';');
					glm::vec3 emission;
					sscanf(token.c_str(), "%f,%f,%f", &emission.r, &emission.g, &emission.b);
					mat->SetEmissionColor(emission, true);
					
					std::getline(iss, token, ';');
					mat->SetEmissionIntensity(std::stof(token), true);
				}
			}
			else if (compData.ComponentType == "LightComponent") {
				auto& light = entity.AddComponent<LightComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';');
				light.SetType((LightType)std::stoi(token));
				
				std::getline(iss, token, ';');
				glm::vec3 color;
				sscanf(token.c_str(), "%f,%f,%f", &color.r, &color.g, &color.b);
				light.SetColor(color);
				
				std::getline(iss, token, ';');
				light.SetIntensity(std::stof(token));
				
				std::getline(iss, token, ';');
				light.SetRange(std::stof(token));
				
				std::getline(iss, token, ';');
				glm::vec3 atten;
				sscanf(token.c_str(), "%f,%f,%f", &atten.x, &atten.y, &atten.z);
				light.SetAttenuation(atten);
				
				std::getline(iss, token, ';');
				light.SetInnerConeAngle(std::stof(token));
				
				std::getline(iss, token, ';');
				light.SetOuterConeAngle(std::stof(token));
				
				std::getline(iss, token, ';');
				light.SetCastShadows(token == "1");
			}
			else if (compData.ComponentType == "Rigidbody2DComponent") {
				auto& rb = entity.AddComponent<Rigidbody2DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';');
				rb.Type = (Rigidbody2DComponent::BodyType)std::stoi(token);
				
				std::getline(iss, token, ';');
				rb.FixedRotation = token == "1";
			}
			else if (compData.ComponentType == "BoxCollider2DComponent") {
				auto& bc = entity.AddComponent<BoxCollider2DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f", &bc.Offset.x, &bc.Offset.y);
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f", &bc.Size.x, &bc.Size.y);
				
				std::getline(iss, token, ';'); bc.Density = std::stof(token);
				std::getline(iss, token, ';'); bc.Friction = std::stof(token);
				std::getline(iss, token, ';'); bc.Restitution = std::stof(token);
				std::getline(iss, token, ';'); bc.RestitutionThreshold = std::stof(token);
			}
			else if (compData.ComponentType == "CircleCollider2DComponent") {
				auto& cc = entity.AddComponent<CircleCollider2DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f", &cc.Offset.x, &cc.Offset.y);
				
				std::getline(iss, token, ';'); cc.Radius = std::stof(token);
				std::getline(iss, token, ';'); cc.Density = std::stof(token);
				std::getline(iss, token, ';'); cc.Friction = std::stof(token);
				std::getline(iss, token, ';'); cc.Restitution = std::stof(token);
				std::getline(iss, token, ';'); cc.RestitutionThreshold = std::stof(token);
			}
			else if (compData.ComponentType == "Rigidbody3DComponent") {
				auto& rb = entity.AddComponent<Rigidbody3DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';'); rb.Type = (Rigidbody3DComponent::BodyType)std::stoi(token);
				std::getline(iss, token, ';'); rb.Mass = std::stof(token);
				std::getline(iss, token, ';'); rb.Friction = std::stof(token);
				std::getline(iss, token, ';'); rb.Restitution = std::stof(token);
				std::getline(iss, token, ';'); rb.LinearDamping = std::stof(token);
				std::getline(iss, token, ';'); rb.AngularDamping = std::stof(token);
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &rb.LinearFactor.x, &rb.LinearFactor.y, &rb.LinearFactor.z);
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &rb.AngularFactor.x, &rb.AngularFactor.y, &rb.AngularFactor.z);
				
				std::getline(iss, token, ';'); rb.UseCCD = token == "1";
				std::getline(iss, token, ';'); rb.CcdMotionThreshold = std::stof(token);
				std::getline(iss, token, ';'); rb.CcdSweptSphereRadius = std::stof(token);
				std::getline(iss, token, ';'); rb.IsTrigger = token == "1";
			}
			else if (compData.ComponentType == "BoxCollider3DComponent") {
				auto& bc = entity.AddComponent<BoxCollider3DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &bc.HalfExtents.x, &bc.HalfExtents.y, &bc.HalfExtents.z);
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &bc.Offset.x, &bc.Offset.y, &bc.Offset.z);
			}
			else if (compData.ComponentType == "SphereCollider3DComponent") {
				auto& sc = entity.AddComponent<SphereCollider3DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';'); sc.Radius = std::stof(token);
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &sc.Offset.x, &sc.Offset.y, &sc.Offset.z);
			}
			else if (compData.ComponentType == "CapsuleCollider3DComponent") {
				auto& cc = entity.AddComponent<CapsuleCollider3DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';'); cc.Radius = std::stof(token);
				std::getline(iss, token, ';'); cc.Height = std::stof(token);
				
				std::getline(iss, token, ';');
				sscanf(token.c_str(), "%f,%f,%f", &cc.Offset.x, &cc.Offset.y, &cc.Offset.z);
			}
			else if (compData.ComponentType == "MeshCollider3DComponent") {
				auto& mc = entity.AddComponent<MeshCollider3DComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';'); mc.Type = (MeshCollider3DComponent::CollisionType)std::stoi(token);
				std::getline(iss, token, ';'); mc.UseEntityMesh = token == "1";
			}
			else if (compData.ComponentType == "ScriptComponent") {
				auto& script = entity.AddComponent<ScriptComponent>();
				
				std::istringstream iss(compData.SerializedData);
				std::string token;
				
				std::getline(iss, token, ';');
				script.AutoCompile = token == "1";
				
				// Parse script paths (separated by |)
				std::getline(iss, token);
				if (!token.empty()) {
					std::istringstream scriptsStream(token);
					std::string scriptPath;
					while (std::getline(scriptsStream, scriptPath, '|')) {
						if (!scriptPath.empty()) {
							script.AddScript(scriptPath);
						}
					}
				}
			}
		}
		
		return entity;
	}

	void Prefab::ApplyTransformOffset(Entity entity, const glm::vec3& offset) {
		if (!entity.HasComponent<TransformComponent>())
			return;
			
		auto& tc = entity.GetComponent<TransformComponent>();
		
		// Calculate relative offset from original position
		glm::vec3 relativeOffset = offset - m_OriginalRootPosition;
		tc.Translation += relativeOffset;
	}

}
