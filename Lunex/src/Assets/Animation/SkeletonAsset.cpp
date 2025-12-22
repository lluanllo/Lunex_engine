#include "stpch.h"
#include "SkeletonAsset.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace YAML {
	// glm::vec3 conversion
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
	
	// glm::quat conversion
	template<>
	struct convert<glm::quat> {
		static Node encode(const glm::quat& rhs) {
			Node node;
			node.push_back(rhs.w);
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}
		
		static bool decode(const Node& node, glm::quat& rhs) {
			if (!node.IsSequence() || node.size() != 4)
				return false;
			rhs.w = node[0].as<float>();
			rhs.x = node[1].as<float>();
			rhs.y = node[2].as<float>();
			rhs.z = node[3].as<float>();
			return true;
		}
	};
	
	// glm::mat4 conversion
	template<>
	struct convert<glm::mat4> {
		static Node encode(const glm::mat4& rhs) {
			Node node;
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					node.push_back(rhs[i][j]);
				}
			}
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}
		
		static bool decode(const Node& node, glm::mat4& rhs) {
			if (!node.IsSequence() || node.size() != 16)
				return false;
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					rhs[i][j] = node[i * 4 + j].as<float>();
				}
			}
			return true;
		}
	};
}

namespace Lunex {

	// ============================================================================
	// SKELETON JOINT
	// ============================================================================
	
	glm::mat4 SkeletonJoint::GetLocalTransform() const {
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), LocalPosition);
		glm::mat4 rotation = glm::mat4_cast(LocalRotation);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), LocalScale);
		return translation * rotation * scale;
	}

	// ============================================================================
	// SKELETON ASSET
	// ============================================================================
	
	SkeletonAsset::SkeletonAsset()
		: Asset("New Skeleton")
	{
	}
	
	SkeletonAsset::SkeletonAsset(const std::string& name)
		: Asset(name)
	{
	}
	
	int32_t SkeletonAsset::AddJoint(const SkeletonJoint& joint) {
		if (m_Joints.size() >= MAX_BONES) {
			LNX_LOG_ERROR("SkeletonAsset: Maximum bone count ({0}) exceeded", MAX_BONES);
			return -1;
		}
		
		int32_t index = static_cast<int32_t>(m_Joints.size());
		m_Joints.push_back(joint);
		m_JointNameToIndex[joint.Name] = index;
		
		if (joint.ParentIndex < 0) {
			m_RootJoints.push_back(index);
		}
		
		MarkDirty();
		return index;
	}
	
	const SkeletonJoint& SkeletonAsset::GetJoint(int32_t index) const {
		LNX_ASSERT(index >= 0 && index < static_cast<int32_t>(m_Joints.size()), "Joint index out of bounds");
		return m_Joints[index];
	}
	
	SkeletonJoint& SkeletonAsset::GetJoint(int32_t index) {
		LNX_ASSERT(index >= 0 && index < static_cast<int32_t>(m_Joints.size()), "Joint index out of bounds");
		return m_Joints[index];
	}
	
	int32_t SkeletonAsset::FindJointIndex(const std::string& name) const {
		auto it = m_JointNameToIndex.find(name);
		if (it != m_JointNameToIndex.end()) {
			return it->second;
		}
		return -1;
	}
	
	const SkeletonJoint* SkeletonAsset::FindJoint(const std::string& name) const {
		int32_t index = FindJointIndex(name);
		if (index >= 0) {
			return &m_Joints[index];
		}
		return nullptr;
	}
	
	std::vector<int32_t> SkeletonAsset::GetChildren(int32_t jointIndex) const {
		std::vector<int32_t> children;
		for (int32_t i = 0; i < static_cast<int32_t>(m_Joints.size()); i++) {
			if (m_Joints[i].ParentIndex == jointIndex) {
				children.push_back(i);
			}
		}
		return children;
	}
	
	bool SkeletonAsset::IsAncestorOf(int32_t ancestorIndex, int32_t descendantIndex) const {
		if (ancestorIndex < 0 || descendantIndex < 0) return false;
		
		int32_t current = descendantIndex;
		while (current >= 0) {
			if (current == ancestorIndex) return true;
			current = m_Joints[current].ParentIndex;
		}
		return false;
	}
	
	void SkeletonAsset::ComputeBindPoses() {
		// Compute bind poses in hierarchical order
		for (int32_t i = 0; i < static_cast<int32_t>(m_Joints.size()); i++) {
			auto& joint = m_Joints[i];
			
			if (joint.ParentIndex < 0) {
				// Root joint - bind pose is just local transform
				joint.BindPose = joint.GetLocalTransform();
			}
			else {
				// Child joint - multiply by parent's bind pose
				joint.BindPose = m_Joints[joint.ParentIndex].BindPose * joint.GetLocalTransform();
			}
		}
	}
	
	void SkeletonAsset::ComputeInverseBindPoses() {
		for (auto& joint : m_Joints) {
			joint.InverseBindPose = glm::inverse(joint.BindPose);
		}
	}
	
	std::vector<glm::mat4> SkeletonAsset::GetBindPoseMatrices() const {
		std::vector<glm::mat4> matrices;
		matrices.reserve(m_Joints.size());
		for (const auto& joint : m_Joints) {
			matrices.push_back(joint.BindPose);
		}
		return matrices;
	}
	
	std::vector<glm::mat4> SkeletonAsset::GetInverseBindPoseMatrices() const {
		std::vector<glm::mat4> matrices;
		matrices.reserve(m_Joints.size());
		for (const auto& joint : m_Joints) {
			matrices.push_back(joint.InverseBindPose);
		}
		return matrices;
	}
	
	void SkeletonAsset::RebuildLookup() {
		m_JointNameToIndex.clear();
		for (int32_t i = 0; i < static_cast<int32_t>(m_Joints.size()); i++) {
			m_JointNameToIndex[m_Joints[i].Name] = i;
		}
	}
	
	void SkeletonAsset::FindRootJoints() {
		m_RootJoints.clear();
		for (int32_t i = 0; i < static_cast<int32_t>(m_Joints.size()); i++) {
			if (m_Joints[i].ParentIndex < 0) {
				m_RootJoints.push_back(i);
			}
		}
	}
	
	// ============================================================================
	// SERIALIZATION
	// ============================================================================
	
	bool SkeletonAsset::SaveToFile(const std::filesystem::path& path) {
		YAML::Emitter out;
		
		out << YAML::BeginMap;
		
		// Header
		out << YAML::Key << "Skeleton" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "ID" << YAML::Value << (uint64_t)GetID();
		out << YAML::Key << "Name" << YAML::Value << GetName();
		out << YAML::Key << "JointCount" << YAML::Value << GetJointCount();
		out << YAML::EndMap;
		
		// Joints
		SerializeJoints(out);
		
		out << YAML::EndMap;
		
		std::ofstream fout(path);
		if (!fout) {
			LNX_LOG_ERROR("SkeletonAsset::SaveToFile - Failed to open file: {0}", path.string());
			return false;
		}
		
		fout << out.c_str();
		fout.close();
		
		SetPath(path);
		ClearDirty();
		
		LNX_LOG_INFO("Skeleton saved: {0} ({1} joints)", path.string(), GetJointCount());
		return true;
	}
	
	Ref<SkeletonAsset> SkeletonAsset::LoadFromFile(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("SkeletonAsset::LoadFromFile - File not found: {0}", path.string());
			return nullptr;
		}
		
		YAML::Node data;
		try {
			data = YAML::LoadFile(path.string());
		}
		catch (const YAML::Exception& e) {
			LNX_LOG_ERROR("SkeletonAsset::LoadFromFile - YAML error: {0}", e.what());
			return nullptr;
		}
		
		auto skeletonNode = data["Skeleton"];
		if (!skeletonNode) {
			LNX_LOG_ERROR("SkeletonAsset::LoadFromFile - Invalid format: {0}", path.string());
			return nullptr;
		}
		
		auto skeleton = CreateRef<SkeletonAsset>();
		skeleton->SetPath(path);
		
		if (skeletonNode["ID"])
			skeleton->SetID(UUID(skeletonNode["ID"].as<uint64_t>()));
		if (skeletonNode["Name"])
			skeleton->SetName(skeletonNode["Name"].as<std::string>());
		
		skeleton->DeserializeJoints(data["Joints"]);
		skeleton->RebuildLookup();
		skeleton->FindRootJoints();
		skeleton->SetLoaded(true);
		
		LNX_LOG_INFO("Skeleton loaded: {0} ({1} joints)", path.string(), skeleton->GetJointCount());
		return skeleton;
	}
	
	void SkeletonAsset::SerializeJoints(YAML::Emitter& out) const {
		out << YAML::Key << "Joints" << YAML::Value << YAML::BeginSeq;
		
		for (int32_t i = 0; i < static_cast<int32_t>(m_Joints.size()); i++) {
			const auto& joint = m_Joints[i];
			
			out << YAML::BeginMap;
			out << YAML::Key << "Index" << YAML::Value << i;
			out << YAML::Key << "Name" << YAML::Value << joint.Name;
			out << YAML::Key << "ParentIndex" << YAML::Value << joint.ParentIndex;
			
			out << YAML::Key << "LocalPosition" << YAML::Value << YAML::Flow
				<< YAML::BeginSeq << joint.LocalPosition.x << joint.LocalPosition.y << joint.LocalPosition.z << YAML::EndSeq;
			out << YAML::Key << "LocalRotation" << YAML::Value << YAML::Flow
				<< YAML::BeginSeq << joint.LocalRotation.w << joint.LocalRotation.x << joint.LocalRotation.y << joint.LocalRotation.z << YAML::EndSeq;
			out << YAML::Key << "LocalScale" << YAML::Value << YAML::Flow
				<< YAML::BeginSeq << joint.LocalScale.x << joint.LocalScale.y << joint.LocalScale.z << YAML::EndSeq;
			
			out << YAML::EndMap;
		}
		
		out << YAML::EndSeq;
	}
	
	void SkeletonAsset::DeserializeJoints(const YAML::Node& node) {
		if (!node || !node.IsSequence()) return;
		
		m_Joints.clear();
		m_Joints.reserve(node.size());
		
		for (const auto& jointNode : node) {
			SkeletonJoint joint;
			
			if (jointNode["Name"])
				joint.Name = jointNode["Name"].as<std::string>();
			if (jointNode["ParentIndex"])
				joint.ParentIndex = jointNode["ParentIndex"].as<int32_t>();
			
			if (jointNode["LocalPosition"]) {
				auto pos = jointNode["LocalPosition"];
				joint.LocalPosition = glm::vec3(pos[0].as<float>(), pos[1].as<float>(), pos[2].as<float>());
			}
			if (jointNode["LocalRotation"]) {
				auto rot = jointNode["LocalRotation"];
				joint.LocalRotation = glm::quat(rot[0].as<float>(), rot[1].as<float>(), rot[2].as<float>(), rot[3].as<float>());
			}
			if (jointNode["LocalScale"]) {
				auto scale = jointNode["LocalScale"];
				joint.LocalScale = glm::vec3(scale[0].as<float>(), scale[1].as<float>(), scale[2].as<float>());
			}
			
			m_Joints.push_back(joint);
		}
		
		// Compute bind poses after loading all joints
		ComputeBindPoses();
		ComputeInverseBindPoses();
	}

}
