#pragma once

/**
 * @file SkeletonAsset.h
 * @brief Skeleton asset for skeletal animation
 * 
 * Part of the AAA Animation System
 * 
 * A skeleton defines the bone hierarchy used for skeletal animation.
 * It contains:
 *   - Bone names and parent indices
 *   - Bind pose transforms (the default pose)
 *   - Inverse bind pose matrices (for skinning)
 */

#include "Assets/Core/Asset.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <unordered_map>

namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex {

	// ============================================================================
	// SKELETON JOINT (BONE)
	// ============================================================================
	
	/**
	 * @struct SkeletonJoint
	 * @brief Represents a single bone in the skeleton hierarchy
	 */
	struct SkeletonJoint {
		std::string Name;                    // Bone name (for lookup)
		int32_t ParentIndex = -1;            // Parent bone index (-1 = root)
		
		// Local transform (relative to parent)
		glm::vec3 LocalPosition = glm::vec3(0.0f);
		glm::quat LocalRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 LocalScale = glm::vec3(1.0f);
		
		// Bind pose (model space transform in T-pose)
		glm::mat4 BindPose = glm::mat4(1.0f);
		
		// Inverse bind pose (for skinning - transforms from model space to bone space)
		glm::mat4 InverseBindPose = glm::mat4(1.0f);
		
		// Get local transform matrix
		glm::mat4 GetLocalTransform() const;
	};

	// ============================================================================
	// SKELETON ASSET (.luskel)
	// ============================================================================
	
	/**
	 * @class SkeletonAsset
	 * @brief Skeleton definition for skeletal meshes
	 * 
	 * Usage:
	 *   1. Import from model file (glTF, FBX)
	 *   2. Save to .luskel format
	 *   3. Assign to SkeletalMeshComponent
	 */
	class SkeletonAsset : public Asset {
	public:
		static constexpr uint32_t MAX_BONES = 256;
		
		SkeletonAsset();
		SkeletonAsset(const std::string& name);
		~SkeletonAsset() = default;

		// Asset interface
		AssetType GetType() const override { return AssetType::Animation; }
		static AssetType StaticType() { return AssetType::Animation; }
		const char* GetExtension() const override { return ".luskel"; }

		// ========== JOINT MANAGEMENT ==========
		
		// Add a joint to the skeleton
		int32_t AddJoint(const SkeletonJoint& joint);
		
		// Get joint by index
		const SkeletonJoint& GetJoint(int32_t index) const;
		SkeletonJoint& GetJoint(int32_t index);
		
		// Get joint by name
		int32_t FindJointIndex(const std::string& name) const;
		const SkeletonJoint* FindJoint(const std::string& name) const;
		
		// Get all joints
		const std::vector<SkeletonJoint>& GetJoints() const { return m_Joints; }
		std::vector<SkeletonJoint>& GetJoints() { return m_Joints; }
		
		// Joint count
		uint32_t GetJointCount() const { return static_cast<uint32_t>(m_Joints.size()); }
		bool IsEmpty() const { return m_Joints.empty(); }
		
		// Get root joint indices (joints with no parent)
		const std::vector<int32_t>& GetRootJoints() const { return m_RootJoints; }
		
		// ========== HIERARCHY ==========
		
		// Get children of a joint
		std::vector<int32_t> GetChildren(int32_t jointIndex) const;
		
		// Check if joint is ancestor of another
		bool IsAncestorOf(int32_t ancestorIndex, int32_t descendantIndex) const;
		
		// ========== BIND POSE ==========
		
		// Compute bind poses from local transforms
		void ComputeBindPoses();
		
		// Compute inverse bind poses
		void ComputeInverseBindPoses();
		
		// Get bind pose matrices (for GPU upload)
		std::vector<glm::mat4> GetBindPoseMatrices() const;
		std::vector<glm::mat4> GetInverseBindPoseMatrices() const;
		
		// ========== SERIALIZATION ==========
		
		bool SaveToFile(const std::filesystem::path& path) override;
		static Ref<SkeletonAsset> LoadFromFile(const std::filesystem::path& path);

	private:
		void RebuildLookup();
		void FindRootJoints();
		
		void SerializeJoints(YAML::Emitter& out) const;
		void DeserializeJoints(const YAML::Node& node);

	private:
		std::vector<SkeletonJoint> m_Joints;
		std::unordered_map<std::string, int32_t> m_JointNameToIndex;
		std::vector<int32_t> m_RootJoints;
	};

}
