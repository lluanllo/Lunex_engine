#include "stpch.h"
#include "SceneSerializer.h"

#include "Entity.h"
#include "Components.h"
#include "Core/JobSystem/JobSystem.h"
#include "Renderer/SkyboxRenderer.h"  // For global skybox serialization

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace YAML {
	template<>
	struct convert<glm::vec2> {
		static Node encode(const glm::vec2& rhs) {
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}
		
		static bool decode(const Node& node, glm::vec2& rhs) {
			if (!node.IsSequence() || node.size() != 2)
				return false;
			
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};
	
	template<>
	struct convert<glm::vec3> {
		static Node encode(const glm::vec3& rhs) {
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}
		
		static bool decode(const Node& node, glm::vec3& rhs) {
			if (!node.IsSequence() || node.size() != 3)
				return false;
			
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};
	
	template<>
	struct convert<glm::vec4> {
		static Node encode(const glm::vec4& rhs) {
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}
		
		static bool decode(const Node& node, glm::vec4& rhs) {
			if (!node.IsSequence() || node.size() != 4)
				return false;
			
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};
}

namespace Lunex {
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
	
	static std::string RigidBody2DBodyTypeToString(Rigidbody2DComponent::BodyType bodyType) {
		switch (bodyType) {
			case Rigidbody2DComponent::BodyType::Static:    return "Static";
			case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
			case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
		}
		
		LNX_CORE_ASSERT(false, "Unknown body type");
		return {};
	}
	
	static Rigidbody2DComponent::BodyType RigidBody2DBodyTypeFromString(const std::string& bodyTypeString) {
		if (bodyTypeString == "Static")    return Rigidbody2DComponent::BodyType::Static;
		if (bodyTypeString == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
		if (bodyTypeString == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;
		
		LNX_CORE_ASSERT(false, "Unknown body type");
		return Rigidbody2DComponent::BodyType::Static;
	}
	
	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}
	
	static void SerializeEntity(YAML::Emitter& out, Entity entity) {
		LNX_ASSERT(entity.HasComponent<IDComponent>());
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID(); // TODO: Entity ID goes here
		
		if (entity.HasComponent<TagComponent>()) {
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent
			
			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;
			
			out << YAML::EndMap; // TagComponent
		}
		
		if (entity.HasComponent<TransformComponent>()) {
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent
			
			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;
			
			out << YAML::EndMap; // TransformComponent
		}
		
		if (entity.HasComponent<CameraComponent>()) {
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent
			
			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;
			
			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera
			
			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;
			
			out << YAML::EndMap; // CameraComponent
		}
		
		if (entity.HasComponent<SpriteRendererComponent>()) {
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap; // SpriteRendererComponent
			
			auto& spriteRendererComponent = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.Color;
			
			if (spriteRendererComponent.Texture)
				out << YAML::Key << "TexturePath" << YAML::Value << spriteRendererComponent.Texture->GetPath();
			
			out << YAML::Key << "TilingFactor" << YAML::Value << spriteRendererComponent.TilingFactor;
			
			out << YAML::EndMap; // SpriteRendererComponent
		}
		
		if (entity.HasComponent<CircleRendererComponent>()) {
			out << YAML::Key << "CircleRendererComponent";
			out << YAML::BeginMap; // CircleRendererComponent
			
			auto& circleRendererComponent = entity.GetComponent<CircleRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << circleRendererComponent.Color;
			out << YAML::Key << "Thickness" << YAML::Value << circleRendererComponent.Thickness;
			out << YAML::Key << "Fade" << YAML::Value << circleRendererComponent.Fade;
			
			out << YAML::EndMap; // CircleRendererComponent
		}
		
		if (entity.HasComponent<Rigidbody2DComponent>()) {
			out << YAML::Key << "Rigidbody2DComponent";
			out << YAML::BeginMap; // Rigidbody2DComponent
			
			auto& rb2dComponent = entity.GetComponent<Rigidbody2DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << RigidBody2DBodyTypeToString(rb2dComponent.Type);
			out << YAML::Key << "FixedRotation" << YAML::Value << rb2dComponent.FixedRotation;
			
			out << YAML::EndMap; // Rigidbody2DComponent
		}
		
		if (entity.HasComponent<BoxCollider2DComponent>()) {
			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap; // BoxCollider2DComponent
			
			auto& bc2dComponent = entity.GetComponent<BoxCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << bc2dComponent.Offset;
			out << YAML::Key << "Size" << YAML::Value << bc2dComponent.Size;
			out << YAML::Key << "Density" << YAML::Value << bc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << bc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << bc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << bc2dComponent.RestitutionThreshold;
			
			out << YAML::EndMap; // BoxCollider2DComponent
		}
		
		if (entity.HasComponent<CircleCollider2DComponent>()) {
			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap; // CircleCollider2DComponent
			
			auto& cc2dComponent = entity.GetComponent<CircleCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << cc2dComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << cc2dComponent.Radius;
			out << YAML::Key << "Density" << YAML::Value << cc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << cc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << cc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << cc2dComponent.RestitutionThreshold;
			
			out << YAML::EndMap; // CircleCollider2DComponent
		}

		// ========================================
		// 3D PHYSICS COMPONENTS
		// ========================================

		if (entity.HasComponent<Rigidbody3DComponent>()) {
			out << YAML::Key << "Rigidbody3DComponent";
			out << YAML::BeginMap; // Rigidbody3DComponent
			
			auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << (int)rb3d.Type;
			out << YAML::Key << "Mass" << YAML::Value << rb3d.Mass;
			out << YAML::Key << "Friction" << YAML::Value << rb3d.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << rb3d.Restitution;
			out << YAML::Key << "LinearDamping" << YAML::Value << rb3d.LinearDamping;
			out << YAML::Key << "AngularDamping" << YAML::Value << rb3d.AngularDamping;
			out << YAML::Key << "LinearFactor" << YAML::Value << rb3d.LinearFactor;
			out << YAML::Key << "AngularFactor" << YAML::Value << rb3d.AngularFactor;
			out << YAML::Key << "UseCCD" << YAML::Value << rb3d.UseCCD;
			out << YAML::Key << "CcdMotionThreshold" << YAML::Value << rb3d.CcdMotionThreshold;
			out << YAML::Key << "CcdSweptSphereRadius" << YAML::Value << rb3d.CcdSweptSphereRadius;
			out << YAML::Key << "IsTrigger" << YAML::Value << rb3d.IsTrigger;
			
			out << YAML::EndMap; // Rigidbody3DComponent
		}

		if (entity.HasComponent<BoxCollider3DComponent>()) {
			out << YAML::Key << "BoxCollider3DComponent";
			out << YAML::BeginMap; // BoxCollider3DComponent
			
			auto& bc3d = entity.GetComponent<BoxCollider3DComponent>();
			out << YAML::Key << "HalfExtents" << YAML::Value << bc3d.HalfExtents;
			out << YAML::Key << "Offset" << YAML::Value << bc3d.Offset;
			
			out << YAML::EndMap; // BoxCollider3DComponent
		}

		if (entity.HasComponent<SphereCollider3DComponent>()) {
			out << YAML::Key << "SphereCollider3DComponent";
			out << YAML::BeginMap; // SphereCollider3DComponent
			
			auto& sc3d = entity.GetComponent<SphereCollider3DComponent>();
			out << YAML::Key << "Radius" << YAML::Value << sc3d.Radius;
			out << YAML::Key << "Offset" << YAML::Value << sc3d.Offset;
			
			out << YAML::EndMap; // SphereCollider3DComponent
		}

		if (entity.HasComponent<CapsuleCollider3DComponent>()) {
			out << YAML::Key << "CapsuleCollider3DComponent";
			out << YAML::BeginMap; // CapsuleCollider3DComponent
			
			auto& cc3d = entity.GetComponent<CapsuleCollider3DComponent>();
			out << YAML::Key << "Radius" << YAML::Value << cc3d.Radius;
			out << YAML::Key << "Height" << YAML::Value << cc3d.Height;
			out << YAML::Key << "Offset" << YAML::Value << cc3d.Offset;
			
			out << YAML::EndMap; // CapsuleCollider3DComponent
		}

		if (entity.HasComponent<MeshCollider3DComponent>()) {
			out << YAML::Key << "MeshCollider3DComponent";
			out << YAML::BeginMap; // MeshCollider3DComponent
			
			auto& mc3d = entity.GetComponent<MeshCollider3DComponent>();
			out << YAML::Key << "Type" << YAML::Value << (int)mc3d.Type;
			out << YAML::Key << "UseEntityMesh" << YAML::Value << mc3d.UseEntityMesh;
			
			out << YAML::EndMap; // MeshCollider3DComponent
		}

		if (entity.HasComponent<MeshComponent>()) {
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap; // MeshComponent
			
			auto& meshComponent = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "Type" << YAML::Value << (int)meshComponent.Type;
			out << YAML::Key << "Color" << YAML::Value << meshComponent.Color;
			
			// ===== NEW: MeshAsset serialization =====
			if (meshComponent.HasMeshAsset()) {
				out << YAML::Key << "MeshAssetID" << YAML::Value << (uint64_t)meshComponent.MeshAssetID;
				out << YAML::Key << "MeshAssetPath" << YAML::Value << meshComponent.MeshAssetPath;
			}
			
			// Legacy: Direct file path (for backwards compatibility)
			out << YAML::Key << "FilePath" << YAML::Value << meshComponent.FilePath;
			
			out << YAML::EndMap; // MeshComponent
		}

		if (entity.HasComponent<MaterialComponent>()) {
			out << YAML::Key << "MaterialComponent";
			out << YAML::BeginMap; // MaterialComponent
			
			auto& materialComponent = entity.GetComponent<MaterialComponent>();
			
			// ===== NEW ARCHITECTURE: Serialize MaterialAsset reference =====
			// Store UUID and path for MaterialAsset lookup
			out << YAML::Key << "MaterialAssetID" << YAML::Value << (uint64_t)materialComponent.GetAssetID();
			out << YAML::Key << "MaterialAssetPath" << YAML::Value << materialComponent.GetAssetPath();
			
			// ===== Serialize local overrides (if any) =====
			// Only save overrides if they exist - this keeps the file clean
			if (materialComponent.HasLocalOverrides()) {
				out << YAML::Key << "HasLocalOverrides" << YAML::Value << true;
				
				// Save current property values (which include overrides)
				out << YAML::Key << "Color" << YAML::Value << materialComponent.GetAlbedo();
				out << YAML::Key << "Metallic" << YAML::Value << materialComponent.GetMetallic();
				out << YAML::Key << "Roughness" << YAML::Value << materialComponent.GetRoughness();
				out << YAML::Key << "Specular" << YAML::Value << materialComponent.GetSpecular();
				out << YAML::Key << "EmissionColor" << YAML::Value << materialComponent.GetEmissionColor();
				out << YAML::Key << "EmissionIntensity" << YAML::Value << materialComponent.GetEmissionIntensity();
			} else {
				out << YAML::Key << "HasLocalOverrides" << YAML::Value << false;
			}
			
			out << YAML::EndMap; // MaterialComponent
		}

		if (entity.HasComponent<LightComponent>()) {
			out << YAML::Key << "LightComponent";
			out << YAML::BeginMap; // LightComponent
			
			auto& lightComponent = entity.GetComponent<LightComponent>();
			out << YAML::Key << "Type" << YAML::Value << (int)lightComponent.GetType();
			out << YAML::Key << "Color" << YAML::Value << lightComponent.GetColor();
			out << YAML::Key << "Intensity" << YAML::Value << lightComponent.GetIntensity();
			out << YAML::Key << "Range" << YAML::Value << lightComponent.GetRange();
			out << YAML::Key << "Attenuation" << YAML::Value << lightComponent.GetAttenuation();
			out << YAML::Key << "InnerConeAngle" << YAML::Value << lightComponent.GetInnerConeAngle();
			out << YAML::Key << "OuterConeAngle" << YAML::Value << lightComponent.GetOuterConeAngle();
			out << YAML::Key << "CastShadows" << YAML::Value << lightComponent.GetCastShadows();
			
			// Sun/Sky settings (for Directional lights)
			out << YAML::Key << "IsSunLight" << YAML::Value << lightComponent.IsSunLight();
			out << YAML::Key << "LinkToSkyboxRotation" << YAML::Value << lightComponent.GetLinkToSkyboxRotation();
			out << YAML::Key << "SkyboxIntensityMultiplier" << YAML::Value << lightComponent.GetSkyboxIntensityMultiplier();
			out << YAML::Key << "AffectAtmosphere" << YAML::Value << lightComponent.GetAffectAtmosphere();
			out << YAML::Key << "AtmosphericDensity" << YAML::Value << lightComponent.GetAtmosphericDensity();
			out << YAML::Key << "RenderSunDisk" << YAML::Value << lightComponent.GetRenderSunDisk();
			out << YAML::Key << "SunDiskSize" << YAML::Value << lightComponent.GetSunDiskSize();
			out << YAML::Key << "SunDiskIntensity" << YAML::Value << lightComponent.GetSunDiskIntensity();
			out << YAML::Key << "ContributeToAmbient" << YAML::Value << lightComponent.GetContributeToAmbient();
			out << YAML::Key << "AmbientContribution" << YAML::Value << lightComponent.GetAmbientContribution();
			out << YAML::Key << "GroundColor" << YAML::Value << lightComponent.GetGroundColor();
			out << YAML::Key << "UseTimeOfDay" << YAML::Value << lightComponent.GetUseTimeOfDay();
			out << YAML::Key << "TimeOfDay" << YAML::Value << lightComponent.GetTimeOfDay();
			out << YAML::Key << "TimeOfDaySpeed" << YAML::Value << lightComponent.GetTimeOfDaySpeed();
			
			out << YAML::EndMap; // LightComponent
		}

		if (entity.HasComponent<TextureComponent>()) {
			out << YAML::Key << "TextureComponent";
			out << YAML::BeginMap; // TextureComponent
			
			auto& textureComponent = entity.GetComponent<TextureComponent>();
			out << YAML::Key << "AlbedoPath" << YAML::Value << textureComponent.AlbedoPath;
			out << YAML::Key << "NormalPath" << YAML::Value << textureComponent.NormalPath;
			out << YAML::Key << "MetallicPath" << YAML::Value << textureComponent.MetallicPath;
			out << YAML::Key << "RoughnessPath" << YAML::Value << textureComponent.RoughnessPath;
			out << YAML::Key << "SpecularPath" << YAML::Value << textureComponent.SpecularPath;
			out << YAML::Key << "EmissionPath" << YAML::Value << textureComponent.EmissionPath;
			out << YAML::Key << "AOPath" << YAML::Value << textureComponent.AOPath;
			out << YAML::Key << "MetallicMultiplier" << YAML::Value << textureComponent.MetallicMultiplier;
			out << YAML::Key << "RoughnessMultiplier" << YAML::Value << textureComponent.RoughnessMultiplier;
			out << YAML::Key << "SpecularMultiplier" << YAML::Value << textureComponent.SpecularMultiplier;
			out << YAML::Key << "AOMultiplier" << YAML::Value << textureComponent.AOMultiplier;
			
			out << YAML::EndMap; // TextureComponent
		}

		if (entity.HasComponent<ScriptComponent>()) {
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap; // ScriptComponent
			
			auto& scriptComponent = entity.GetComponent<ScriptComponent>();
			
			// ===== SERIALIZAR MÚLTIPLES SCRIPTS =====
			out << YAML::Key << "Scripts" << YAML::Value << YAML::BeginSeq;
			for (size_t i = 0; i < scriptComponent.GetScriptCount(); i++) {
				out << YAML::BeginMap;
				out << YAML::Key << "ScriptPath" << YAML::Value << scriptComponent.GetScriptPath(i);
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
			
			out << YAML::Key << "AutoCompile" << YAML::Value << scriptComponent.AutoCompile;
			
			out << YAML::EndMap; // ScriptComponent
		}

		// ========================================
		// RELATIONSHIP COMPONENT (Parent-Child Hierarchy)
		// ========================================
		if (entity.HasComponent<RelationshipComponent>()) {
			auto& rel = entity.GetComponent<RelationshipComponent>();
			
			// Only serialize if has parent or children
			if (rel.HasParent() || rel.HasChildren()) {
				out << YAML::Key << "RelationshipComponent";
				out << YAML::BeginMap;
				
				out << YAML::Key << "ParentID" << YAML::Value << (uint64_t)rel.ParentID;
				
				out << YAML::Key << "ChildrenIDs" << YAML::Value << YAML::BeginSeq;
				for (UUID childID : rel.ChildrenIDs) {
					out << (uint64_t)childID;
				}
				out << YAML::EndSeq;
				
				out << YAML::EndMap;
			}
		}
		
		// ========================================
		// ENVIRONMENT COMPONENT (Skybox/HDRI/IBL)
		// ========================================
		if (entity.HasComponent<EnvironmentComponent>()) {
			out << YAML::Key << "EnvironmentComponent";
			out << YAML::BeginMap; // EnvironmentComponent
			
			auto& envComp = entity.GetComponent<EnvironmentComponent>();
			out << YAML::Key << "HDRIPath" << YAML::Value << envComp.HDRIPath;
			out << YAML::Key << "IsActive" << YAML::Value << envComp.IsActive;
			out << YAML::Key << "RenderSkybox" << YAML::Value << envComp.RenderSkybox;
			out << YAML::Key << "UseForLighting" << YAML::Value << envComp.UseForLighting;
			out << YAML::Key << "CubemapResolution" << YAML::Value << envComp.CubemapResolution;
			out << YAML::Key << "Intensity" << YAML::Value << envComp.GetIntensity();
			out << YAML::Key << "Rotation" << YAML::Value << envComp.GetRotation();
			out << YAML::Key << "Tint" << YAML::Value << envComp.GetTint();
			out << YAML::Key << "Blur" << YAML::Value << envComp.GetBlur();
			
			out << YAML::EndMap; // EnvironmentComponent
		}
		
		out << YAML::EndMap; // Entity
	}
	
	void SceneSerializer::Serialize(const std::string& filepath) {
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << "Untitled";
		
		// ========================================
		// SERIALIZE GLOBAL SKYBOX/HDRI SETTINGS
		// ========================================
		out << YAML::Key << "GlobalSkybox" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Enabled" << YAML::Value << SkyboxRenderer::IsEnabled();
		
		if (SkyboxRenderer::HasEnvironmentLoaded()) {
			std::string hdriPath = SkyboxRenderer::GetHDRIPath();
			if (!hdriPath.empty()) {
				out << YAML::Key << "HDRIPath" << YAML::Value << hdriPath;
				out << YAML::Key << "Intensity" << YAML::Value << SkyboxRenderer::GetIntensity();
				out << YAML::Key << "Rotation" << YAML::Value << SkyboxRenderer::GetRotation();
				out << YAML::Key << "Tint" << YAML::Value << SkyboxRenderer::GetTint();
				out << YAML::Key << "Blur" << YAML::Value << SkyboxRenderer::GetBlur();
			}
		} else {
			// Save background color when no HDRI is loaded
			out << YAML::Key << "HDRIPath" << YAML::Value << "";
			out << YAML::Key << "BackgroundColor" << YAML::Value << SkyboxRenderer::GetBackgroundColor();
		}
		
		out << YAML::EndMap; // GlobalSkybox
		
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		
		m_Scene->m_Registry.view<entt::entity>().each([&](auto entityID) {
			Entity entity = { entityID, m_Scene.get() };
			if (!entity)
				return;

			SerializeEntity(out, entity);
		});
		
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
		
		LNX_LOG_INFO("Scene serialized to: {0}", filepath);
		LNX_LOG_INFO("  - Global Skybox: {0}", SkyboxRenderer::IsEnabled() ? "Enabled" : "Disabled");
		if (SkyboxRenderer::HasEnvironmentLoaded()) {
			LNX_LOG_INFO("  - HDRI loaded: Yes");
		}
	}
	
	void SceneSerializer::SerializeRuntime(const std::string& filepath) {
		// Not implemented
		LNX_CORE_ASSERT(false);
	}
	
	bool SceneSerializer::Deserialize(const std::string& filepath) {
		YAML::Node data;
		try {
			data = YAML::LoadFile(filepath);
		}
		catch (YAML::ParserException e) {
			return false;
		}
		
		if (!data["Scene"])
			return false;
		
		std::string sceneName = data["Scene"].as<std::string>();
		LNX_LOG_TRACE("Deserializing scene '{0}'", sceneName);
		
		// ========================================
		// DESERIALIZE GLOBAL SKYBOX/HDRI SETTINGS
		// ========================================
		auto globalSkybox = data["GlobalSkybox"];
		if (globalSkybox) {
			if (globalSkybox["Enabled"]) {
				SkyboxRenderer::SetEnabled(globalSkybox["Enabled"].as<bool>());
			}
			
			if (globalSkybox["HDRIPath"]) {
				std::string hdriPath = globalSkybox["HDRIPath"].as<std::string>();
				
				if (!hdriPath.empty()) {
					// Load HDRI
					bool loaded = SkyboxRenderer::LoadHDRI(hdriPath);
					
					if (loaded) {
						// Apply settings
						if (globalSkybox["Intensity"]) {
							SkyboxRenderer::SetIntensity(globalSkybox["Intensity"].as<float>());
						}
						if (globalSkybox["Rotation"]) {
							SkyboxRenderer::SetRotation(globalSkybox["Rotation"].as<float>());
						}
						if (globalSkybox["Tint"]) {
							SkyboxRenderer::SetTint(globalSkybox["Tint"].as<glm::vec3>());
						}
						if (globalSkybox["Blur"]) {
							SkyboxRenderer::SetBlur(globalSkybox["Blur"].as<float>());
						}
						
						LNX_LOG_INFO("Loaded global HDRI: {0}", hdriPath);
					} else {
						LNX_LOG_WARN("Failed to load HDRI from: {0}", hdriPath);
					}
				} else {
					// No HDRI, use background color
					if (globalSkybox["BackgroundColor"]) {
						SkyboxRenderer::SetBackgroundColor(globalSkybox["BackgroundColor"].as<glm::vec3>());
					}
				}
			}
		} else {
			// No skybox data - use defaults
			SkyboxRenderer::SetEnabled(false);
			LNX_LOG_INFO("No global skybox data found, using defaults");
		}
		
		std::unordered_set<uint64_t> seenIds;
		
		auto entities = data["Entities"];
		if (entities) {
			for (auto entity : entities) {
				uint64_t uuid = entity["Entity"].as<uint64_t>();
				
				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();
				
				LNX_LOG_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);
				
				// Si el ID ya apareció, generar uno nuevo único
				if (!seenIds.insert(uuid).second) {
					uint64_t newUuid;
					do {
						newUuid = (uint64_t)UUID();
					} while (!seenIds.insert(newUuid).second);
					LNX_LOG_WARN("Duplicate Entity ID {0} ('{1}') detected. Generated new ID {2}.", uuid, name, newUuid);
					uuid = newUuid;
				}
				
				LNX_LOG_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);
				
				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);
				
				auto transformComponent = entity["TransformComponent"];
				if (transformComponent) {
					// Entities always have transforms
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Translation"].as<glm::vec3>();
					tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
					tc.Scale = transformComponent["Scale"].as<glm::vec3>();
				}
				
				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent) {
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();
					
					auto cameraProps = cameraComponent["Camera"];
					cc.Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProps["ProjectionType"].as<int>());
					
					cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFOV"].as<float>());
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());
					
					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());
					
					cc.Primary = cameraComponent["Primary"].as<bool>();
					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
				}
				
				auto spriteRendererComponent = entity["SpriteRendererComponent"];
				if (spriteRendererComponent) {
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<glm::vec4>();
					if (spriteRendererComponent["TexturePath"])
						src.Texture = Texture2D::Create(spriteRendererComponent["TexturePath"].as<std::string>());
					
					if (spriteRendererComponent["TilingFactor"])
						src.TilingFactor = spriteRendererComponent["TilingFactor"].as<float>();
				}
				
				auto circleRendererComponent = entity["CircleRendererComponent"];
				if (circleRendererComponent) {
					auto& crc = deserializedEntity.AddComponent<CircleRendererComponent>();
					crc.Color = circleRendererComponent["Color"].as<glm::vec4>();
					crc.Thickness = circleRendererComponent["Thickness"].as<float>();
					crc.Fade = circleRendererComponent["Fade"].as<float>();
				}
				
				auto rigidbody2DComponent = entity["Rigidbody2DComponent"];
				if (rigidbody2DComponent) {
					auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
					rb2d.Type = RigidBody2DBodyTypeFromString(rigidbody2DComponent["BodyType"].as<std::string>());
					rb2d.FixedRotation = rigidbody2DComponent["FixedRotation"].as<bool>();
				}
				
				auto boxCollider2DComponent = entity["BoxCollider2DComponent"];
				if (boxCollider2DComponent) {
					auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
					bc2d.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>();
					bc2d.Size = boxCollider2DComponent["Size"].as<glm::vec2>();
					bc2d.Density = boxCollider2DComponent["Density"].as<float>();
					bc2d.Friction = boxCollider2DComponent["Friction"].as<float>();
					bc2d.Restitution = boxCollider2DComponent["Restitution"].as<float>();
					bc2d.RestitutionThreshold = boxCollider2DComponent["RestitutionThreshold"].as<float>();
				}
				
				auto circleCollider2DComponent = entity["CircleCollider2DComponent"];
				if (circleCollider2DComponent) {
					auto& cc2d = deserializedEntity.AddComponent<CircleCollider2DComponent>();
					cc2d.Offset = circleCollider2DComponent["Offset"].as<glm::vec2>();
					cc2d.Radius = circleCollider2DComponent["Radius"].as<float>();
					cc2d.Density = circleCollider2DComponent["Density"].as<float>();
					cc2d.Friction = circleCollider2DComponent["Friction"].as<float>();
					cc2d.Restitution = circleCollider2DComponent["Restitution"].as<float>();
					cc2d.RestitutionThreshold = circleCollider2DComponent["RestitutionThreshold"].as<float>();
				}

				// ========================================
				// 3D PHYSICS COMPONENTS
				// ========================================

				auto rigidbody3DComponent = entity["Rigidbody3DComponent"];
				if (rigidbody3DComponent) {
					auto& rb3d = deserializedEntity.AddComponent<Rigidbody3DComponent>();
					rb3d.Type = (Rigidbody3DComponent::BodyType)rigidbody3DComponent["BodyType"].as<int>();
					rb3d.Mass = rigidbody3DComponent["Mass"].as<float>();
					rb3d.Friction = rigidbody3DComponent["Friction"].as<float>();
					rb3d.Restitution = rigidbody3DComponent["Restitution"].as<float>();
					rb3d.LinearDamping = rigidbody3DComponent["LinearDamping"].as<float>();
					rb3d.AngularDamping = rigidbody3DComponent["AngularDamping"].as<float>();
					rb3d.LinearFactor = rigidbody3DComponent["LinearFactor"].as<glm::vec3>();
					rb3d.AngularFactor = rigidbody3DComponent["AngularFactor"].as<glm::vec3>();
					rb3d.UseCCD = rigidbody3DComponent["UseCCD"].as<bool>();
					rb3d.CcdMotionThreshold = rigidbody3DComponent["CcdMotionThreshold"].as<float>();
					rb3d.CcdSweptSphereRadius = rigidbody3DComponent["CcdSweptSphereRadius"].as<float>();
					rb3d.IsTrigger = rigidbody3DComponent["IsTrigger"].as<bool>();
				}

				auto boxCollider3DComponent = entity["BoxCollider3DComponent"];
				if (boxCollider3DComponent) {
					auto& bc3d = deserializedEntity.AddComponent<BoxCollider3DComponent>();
					bc3d.HalfExtents = boxCollider3DComponent["HalfExtents"].as<glm::vec3>();
					bc3d.Offset = boxCollider3DComponent["Offset"].as<glm::vec3>();
				}

				auto sphereCollider3DComponent = entity["SphereCollider3DComponent"];
				if (sphereCollider3DComponent) {
					auto& sc3d = deserializedEntity.AddComponent<SphereCollider3DComponent>();
					sc3d.Radius = sphereCollider3DComponent["Radius"].as<float>();
					sc3d.Offset = sphereCollider3DComponent["Offset"].as<glm::vec3>();
				}

				auto capsuleCollider3DComponent = entity["CapsuleCollider3DComponent"];
				if (capsuleCollider3DComponent) {
					auto& cc3d = deserializedEntity.AddComponent<CapsuleCollider3DComponent>();
					cc3d.Radius = capsuleCollider3DComponent["Radius"].as<float>();
					cc3d.Height = capsuleCollider3DComponent["Height"].as<float>();
					cc3d.Offset = capsuleCollider3DComponent["Offset"].as<glm::vec3>();
				}

				auto meshCollider3DComponent = entity["MeshCollider3DComponent"];
				if (meshCollider3DComponent) {
					auto& mc3d = deserializedEntity.AddComponent<MeshCollider3DComponent>();
					mc3d.Type = (MeshCollider3DComponent::CollisionType)meshCollider3DComponent["Type"].as<int>();
					mc3d.UseEntityMesh = meshCollider3DComponent["UseEntityMesh"].as<bool>();
				}

				auto meshComponent = entity["MeshComponent"];
				if (meshComponent) {
					auto& mc = deserializedEntity.AddComponent<MeshComponent>();
					mc.Type = (ModelType)meshComponent["Type"].as<int>();
					mc.Color = meshComponent["Color"].as<glm::vec4>();
					
					// ===== NEW: MeshAsset deserialization =====
					if (meshComponent["MeshAssetID"] && meshComponent["MeshAssetPath"]) {
						uint64_t assetID = meshComponent["MeshAssetID"].as<uint64_t>();
						std::string assetPath = meshComponent["MeshAssetPath"].as<std::string>();
						
						if (!assetPath.empty()) {
							// Try to load MeshAsset from path
							Ref<MeshAsset> meshAsset = MeshAsset::LoadFromFile(assetPath);
							if (meshAsset) {
								mc.SetMeshAsset(meshAsset);
							}
							else {
								LNX_LOG_WARN("MeshAsset with UUID {0} at path '{1}' not found", assetID, assetPath);
								// Store the path for potential later loading
								mc.MeshAssetID = UUID(assetID);
								mc.MeshAssetPath = assetPath;
							}
						}
					}
					// Legacy: Direct file path loading
					else if (meshComponent["FilePath"]) {
						mc.FilePath = meshComponent["FilePath"].as<std::string>();
						if (!mc.FilePath.empty() && mc.Type == ModelType::FromFile) {
							mc.LoadFromFile(mc.FilePath);
						}
					}
					
					// If it's a primitive type, create it
					if (mc.Type != ModelType::FromFile) {
						mc.CreatePrimitive(mc.Type);
					}
				}

				auto materialComponent = entity["MaterialComponent"];
				if (materialComponent) {
					// MeshComponent already created MaterialComponent automatically
					// So we need to get it instead of adding a new one
					MaterialComponent* mat = nullptr;
					
					if (deserializedEntity.HasComponent<MaterialComponent>()) {
						mat = &deserializedEntity.GetComponent<MaterialComponent>();
					} else {
						mat = &deserializedEntity.AddComponent<MaterialComponent>();
					}
					
					// ===== NEW ARCHITECTURE: Load MaterialAsset by UUID/Path =====
					if (materialComponent["MaterialAssetID"]) {
						uint64_t assetID = materialComponent["MaterialAssetID"].as<uint64_t>();
						UUID materialUUID(assetID);
						
						// Try to load material from registry by UUID
						auto& registry = MaterialRegistry::Get();
						Ref<MaterialAsset> materialAsset = registry.GetMaterial(materialUUID);
						
						// If not found by UUID, try loading by path
						if (!materialAsset && materialComponent["MaterialAssetPath"]) {
							std::string assetPath = materialComponent["MaterialAssetPath"].as<std::string>();
							if (!assetPath.empty()) {
								materialAsset = registry.LoadMaterial(assetPath);
							}
						}
						
						// If material asset was found, assign it
						if (materialAsset) {
							mat->SetMaterialAsset(materialAsset);
						} else {
							LNX_LOG_WARN("MaterialAsset with UUID {0} not found, using default material", assetID);
							// Component will keep its default material from constructor
						}
					}
					
					// ===== Restore local overrides if present =====
					if (materialComponent["HasLocalOverrides"] && materialComponent["HasLocalOverrides"].as<bool>()) {
						// Apply overrides (asOverride = true to create local overrides)
						if (materialComponent["Color"]) {
							mat->SetAlbedo(materialComponent["Color"].as<glm::vec4>(), true);
						}
						if (materialComponent["Metallic"]) {
							mat->SetMetallic(materialComponent["Metallic"].as<float>(), true);
						}
						if (materialComponent["Roughness"]) {
							mat->SetRoughness(materialComponent["Roughness"].as<float>(), true);
						}
						if (materialComponent["Specular"]) {
							mat->SetSpecular(materialComponent["Specular"].as<float>(), true);
						}
						if (materialComponent["EmissionColor"]) {
							mat->SetEmissionColor(materialComponent["EmissionColor"].as<glm::vec3>(), true);
						}
						if (materialComponent["EmissionIntensity"]) {
							mat->SetEmissionIntensity(materialComponent["EmissionIntensity"].as<float>(), true);
						}
					}
					// ===== LEGACY COMPATIBILITY: Old format (no MaterialAssetID) =====
					else if (!materialComponent["MaterialAssetID"] && materialComponent["Color"]) {
						// Old scene format - apply values as overrides to default material
						mat->SetAlbedo(materialComponent["Color"].as<glm::vec4>(), true);
						mat->SetMetallic(materialComponent["Metallic"].as<float>(), true);
						mat->SetRoughness(materialComponent["Roughness"].as<float>(), true);
						mat->SetSpecular(materialComponent["Specular"].as<float>(), true);
						mat->SetEmissionColor(materialComponent["EmissionColor"].as<glm::vec3>(), true);
						mat->SetEmissionIntensity(materialComponent["EmissionIntensity"].as<float>(), true);
					}
				}

				auto lightComponent = entity["LightComponent"];
				if (lightComponent) {
					auto& light = deserializedEntity.AddComponent<LightComponent>();
					light.SetType((LightType)lightComponent["Type"].as<int>());
					light.SetColor(lightComponent["Color"].as<glm::vec3>());
					light.SetIntensity(lightComponent["Intensity"].as<float>());
					light.SetRange(lightComponent["Range"].as<float>());
					light.SetAttenuation(lightComponent["Attenuation"].as<glm::vec3>());
					light.SetInnerConeAngle(lightComponent["InnerConeAngle"].as<float>());
					light.SetOuterConeAngle(lightComponent["OuterConeAngle"].as<float>());
					light.SetCastShadows(lightComponent["CastShadows"].as<bool>());
					
					// Sun/Sky settings (for Directional lights)
					if (lightComponent["IsSunLight"])
						light.SetIsSunLight(lightComponent["IsSunLight"].as<bool>());
					if (lightComponent["LinkToSkyboxRotation"])
						light.SetLinkToSkyboxRotation(lightComponent["LinkToSkyboxRotation"].as<bool>());
					if (lightComponent["SkyboxIntensityMultiplier"])
						light.SetSkyboxIntensityMultiplier(lightComponent["SkyboxIntensityMultiplier"].as<float>());
					if (lightComponent["AffectAtmosphere"])
						light.SetAffectAtmosphere(lightComponent["AffectAtmosphere"].as<bool>());
					if (lightComponent["AtmosphericDensity"])
						light.SetAtmosphericDensity(lightComponent["AtmosphericDensity"].as<float>());
					if (lightComponent["RenderSunDisk"])
						light.SetRenderSunDisk(lightComponent["RenderSunDisk"].as<bool>());
					if (lightComponent["SunDiskSize"])
						light.SetSunDiskSize(lightComponent["SunDiskSize"].as<float>());
					if (lightComponent["SunDiskIntensity"])
						light.SetSunDiskIntensity(lightComponent["SunDiskIntensity"].as<float>());
					if (lightComponent["ContributeToAmbient"])
						light.SetContributeToAmbient(lightComponent["ContributeToAmbient"].as<bool>());
					if (lightComponent["AmbientContribution"])
						light.SetAmbientContribution(lightComponent["AmbientContribution"].as<float>());
					if (lightComponent["GroundColor"])
						light.SetGroundColor(lightComponent["GroundColor"].as<glm::vec3>());
					if (lightComponent["UseTimeOfDay"])
						light.SetUseTimeOfDay(lightComponent["UseTimeOfDay"].as<bool>());
					if (lightComponent["TimeOfDay"])
						light.SetTimeOfDay(lightComponent["TimeOfDay"].as<float>());
					if (lightComponent["TimeOfDaySpeed"])
						light.SetTimeOfDaySpeed(lightComponent["TimeOfDaySpeed"].as<float>());
				}

				auto textureComponent = entity["TextureComponent"];
				if (textureComponent) {
					auto& texture = deserializedEntity.AddComponent<TextureComponent>();
					
					// Load albedo
					if (textureComponent["AlbedoPath"]) {
						std::string path = textureComponent["AlbedoPath"].as<std::string>();
						if (!path.empty()) {
							texture.LoadAlbedo(path);
						}
					}
					
					// Load normal
					if (textureComponent["NormalPath"]) {
						std::string path = textureComponent["NormalPath"].as<std::string>();
						if (!path.empty()) {
							texture.LoadNormal(path);
						}
					}
					
					// Load metallic
					if (textureComponent["MetallicPath"]) {
						std::string path = textureComponent["MetallicPath"].as<std::string>();
						if (!path.empty()) {
							texture.LoadMetallic(path);
						}
					}
					
					// Load roughness
					if (textureComponent["RoughnessPath"]) {
						std::string path = textureComponent["RoughnessPath"].as<std::string>();
						if (!path.empty()) {
							texture.LoadRoughness(path);
						}
					}
					
					// Load specular
					if (textureComponent["SpecularPath"]) {
						std::string path = textureComponent["SpecularPath"].as<std::string>();
						if (!path.empty()) {
							texture.LoadSpecular(path);
						}
					}
					
					// Load emission
					if (textureComponent["EmissionPath"]) {
						std::string path = textureComponent["EmissionPath"].as<std::string>();
						if (!path.empty()) {
							texture.LoadEmission(path);
						}
					}
					
					// Load AO
					if (textureComponent["AOPath"]) {
						std::string path = textureComponent["AOPath"].as<std::string>();
						if (!path.empty()) {
							texture.LoadAO(path);
						}
					}
					
					// Load multipliers
					if (textureComponent["MetallicMultiplier"])
						texture.MetallicMultiplier = textureComponent["MetallicMultiplier"].as<float>();
					if (textureComponent["RoughnessMultiplier"])
						texture.RoughnessMultiplier = textureComponent["RoughnessMultiplier"].as<float>();
					if (textureComponent["SpecularMultiplier"])
						texture.SpecularMultiplier = textureComponent["SpecularMultiplier"].as<float>();
					if (textureComponent["AOMultiplier"])
						texture.AOMultiplier = textureComponent["AOMultiplier"].as<float>();
				}

				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent) {
					auto& script = deserializedEntity.AddComponent<ScriptComponent>();
					
					// ===== DESERIALIZAR MÚLTIPLES SCRIPTS =====
					if (scriptComponent["Scripts"] && scriptComponent["Scripts"].IsSequence()) {
						// Nuevo formato (múltiples scripts)
						for (auto scriptNode : scriptComponent["Scripts"]) {
							if (scriptNode["ScriptPath"]) {
								std::string scriptPath = scriptNode["ScriptPath"].as<std::string>();
								if (!scriptPath.empty()) {
									script.AddScript(scriptPath);
								}
							}
						}
					}
					else if (scriptComponent["ScriptPath"]) {
						// Formato antiguo (compatibilidad hacia atrás - un solo script)
						std::string scriptPath = scriptComponent["ScriptPath"].as<std::string>();
						if (!scriptPath.empty()) {
							script.AddScript(scriptPath);
						}
					}
					
					if (scriptComponent["AutoCompile"]) {
						script.AutoCompile = scriptComponent["AutoCompile"].as<bool>();
					}
				}

				// ========================================
				// RELATIONSHIP COMPONENT (Parent-Child Hierarchy)
				// ========================================
				auto relationshipComponent = entity["RelationshipComponent"];
				if (relationshipComponent) {
					auto& rel = deserializedEntity.AddComponent<RelationshipComponent>();
					
					if (relationshipComponent["ParentID"]) {
						rel.ParentID = UUID(relationshipComponent["ParentID"].as<uint64_t>());
					}
					
					if (relationshipComponent["ChildrenIDs"] && relationshipComponent["ChildrenIDs"].IsSequence()) {
						for (auto childIDNode : relationshipComponent["ChildrenIDs"]) {
							rel.ChildrenIDs.push_back(UUID(childIDNode.as<uint64_t>()));
						}
					}
				}

				// ========================================
				// ENVIRONMENT COMPONENT (Skybox/HDRI/IBL)
				// ========================================
				auto environmentComponent = entity["EnvironmentComponent"];
				if (environmentComponent) {
					auto& envComp = deserializedEntity.AddComponent<EnvironmentComponent>();
					
					// Load settings first (before loading HDRI to set resolution)
					if (environmentComponent["CubemapResolution"])
						envComp.CubemapResolution = environmentComponent["CubemapResolution"].as<uint32_t>();
					
					if (environmentComponent["IsActive"])
						envComp.IsActive = environmentComponent["IsActive"].as<bool>();
					
					if (environmentComponent["RenderSkybox"])
						envComp.RenderSkybox = environmentComponent["RenderSkybox"].as<bool>();
					
					if (environmentComponent["UseForLighting"])
						envComp.UseForLighting = environmentComponent["UseForLighting"].as<bool>();
					
					// Load HDRI if path is specified
					if (environmentComponent["HDRIPath"]) {
						std::string hdriPath = environmentComponent["HDRIPath"].as<std::string>();
						if (!hdriPath.empty()) {
							envComp.LoadHDRI(hdriPath);
						}
					}
					
					// Apply environment settings (after loading)
					if (environmentComponent["Intensity"])
						envComp.SetIntensity(environmentComponent["Intensity"].as<float>());
					
					if (environmentComponent["Rotation"])
						envComp.SetRotation(environmentComponent["Rotation"].as<float>());
					
					if (environmentComponent["Tint"])
						envComp.SetTint(environmentComponent["Tint"].as<glm::vec3>());
					
					if (environmentComponent["Blur"])
						envComp.SetBlur(environmentComponent["Blur"].as<float>());
				}
			}
		}
		
		return true;
	}
}