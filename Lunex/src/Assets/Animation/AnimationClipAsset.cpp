#include "stpch.h"
#include "AnimationClipAsset.h"
#include "SkeletonAsset.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <algorithm>

namespace Lunex {

	// ============================================================================
	// ANIMATION CHANNEL
	// ============================================================================
	
	AnimationKeyframe AnimationChannel::Sample(float time) const {
		if (Keyframes.empty()) {
			return AnimationKeyframe();
		}
		
		if (Keyframes.size() == 1) {
			return Keyframes[0];
		}
		
		// Find surrounding keyframes
		size_t nextIndex = 0;
		for (size_t i = 0; i < Keyframes.size(); i++) {
			if (Keyframes[i].Time > time) {
				nextIndex = i;
				break;
			}
			nextIndex = i;
		}
		
		if (nextIndex == 0) {
			return Keyframes[0];
		}
		
		size_t prevIndex = nextIndex - 1;
		const auto& prev = Keyframes[prevIndex];
		const auto& next = Keyframes[nextIndex];
		
		// Calculate interpolation factor
		float duration = next.Time - prev.Time;
		float t = (duration > 0.0f) ? (time - prev.Time) / duration : 0.0f;
		t = glm::clamp(t, 0.0f, 1.0f);
		
		AnimationKeyframe result;
		result.Time = time;
		
		switch (prev.Interpolation) {
			case AnimationKeyframe::InterpolationType::Step:
				result.Translation = prev.Translation;
				result.Rotation = prev.Rotation;
				result.Scale = prev.Scale;
				break;
				
			case AnimationKeyframe::InterpolationType::Linear:
			default:
				result.Translation = glm::mix(prev.Translation, next.Translation, t);
				result.Rotation = glm::slerp(prev.Rotation, next.Rotation, t);
				result.Scale = glm::mix(prev.Scale, next.Scale, t);
				break;
				
			case AnimationKeyframe::InterpolationType::Cubic:
				// TODO: Implement cubic spline interpolation
				result.Translation = glm::mix(prev.Translation, next.Translation, t);
				result.Rotation = glm::slerp(prev.Rotation, next.Rotation, t);
				result.Scale = glm::mix(prev.Scale, next.Scale, t);
				break;
		}
		
		return result;
	}
	
	float AnimationChannel::GetDuration() const {
		if (Keyframes.empty()) return 0.0f;
		return Keyframes.back().Time;
	}
	
	void AnimationChannel::SortKeyframes() {
		std::sort(Keyframes.begin(), Keyframes.end(),
			[](const AnimationKeyframe& a, const AnimationKeyframe& b) {
				return a.Time < b.Time;
			});
	}

	// ============================================================================
	// ANIMATION CLIP ASSET
	// ============================================================================
	
	AnimationClipAsset::AnimationClipAsset()
		: Asset("New Animation")
	{
	}
	
	AnimationClipAsset::AnimationClipAsset(const std::string& name)
		: Asset(name)
	{
	}
	
	void AnimationClipAsset::AddChannel(const AnimationChannel& channel) {
		m_Channels.push_back(channel);
		MarkDirty();
	}
	
	const AnimationChannel* AnimationClipAsset::GetChannel(const std::string& jointName) const {
		for (const auto& channel : m_Channels) {
			if (channel.JointName == jointName) {
				return &channel;
			}
		}
		return nullptr;
	}
	
	AnimationChannel* AnimationClipAsset::GetChannel(const std::string& jointName) {
		for (auto& channel : m_Channels) {
			if (channel.JointName == jointName) {
				return &channel;
			}
		}
		return nullptr;
	}
	
	const AnimationChannel& AnimationClipAsset::GetChannel(size_t index) const {
		LNX_ASSERT(index < m_Channels.size(), "Channel index out of bounds");
		return m_Channels[index];
	}
	
	AnimationChannel& AnimationClipAsset::GetChannel(size_t index) {
		LNX_ASSERT(index < m_Channels.size(), "Channel index out of bounds");
		return m_Channels[index];
	}
	
	void AnimationClipAsset::Sample(float time, std::vector<AnimationKeyframe>& outPose) const {
		float normalizedTime = NormalizeTime(time);
		
		for (const auto& channel : m_Channels) {
			if (channel.JointIndex >= 0 && channel.JointIndex < static_cast<int32_t>(outPose.size())) {
				outPose[channel.JointIndex] = channel.Sample(normalizedTime);
			}
		}
	}
	
	void AnimationClipAsset::ResolveJointIndices(const SkeletonAsset& skeleton) {
		for (auto& channel : m_Channels) {
			channel.JointIndex = skeleton.FindJointIndex(channel.JointName);
			if (channel.JointIndex < 0) {
				LNX_LOG_WARN("AnimationClipAsset: Joint '{0}' not found in skeleton", channel.JointName);
			}
		}
	}
	
	void AnimationClipAsset::ComputeDuration() {
		m_Duration = 0.0f;
		for (const auto& channel : m_Channels) {
			float channelDuration = channel.GetDuration();
			if (channelDuration > m_Duration) {
				m_Duration = channelDuration;
			}
		}
	}
	
	float AnimationClipAsset::NormalizeTime(float time) const {
		if (m_Duration <= 0.0f) return 0.0f;
		
		if (m_Loop) {
			return fmod(time, m_Duration);
		}
		else {
			return glm::clamp(time, 0.0f, m_Duration);
		}
	}
	
	// ============================================================================
	// SERIALIZATION
	// ============================================================================
	
	bool AnimationClipAsset::SaveToFile(const std::filesystem::path& path) {
		YAML::Emitter out;
		
		out << YAML::BeginMap;
		
		// Header
		out << YAML::Key << "AnimationClip" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "ID" << YAML::Value << (uint64_t)GetID();
		out << YAML::Key << "Name" << YAML::Value << GetName();
		out << YAML::Key << "Duration" << YAML::Value << m_Duration;
		out << YAML::Key << "TicksPerSecond" << YAML::Value << m_TicksPerSecond;
		out << YAML::Key << "Loop" << YAML::Value << m_Loop;
		out << YAML::Key << "ChannelCount" << YAML::Value << m_Channels.size();
		out << YAML::EndMap;
		
		// Channels
		SerializeChannels(out);
		
		out << YAML::EndMap;
		
		std::ofstream fout(path);
		if (!fout) {
			LNX_LOG_ERROR("AnimationClipAsset::SaveToFile - Failed to open file: {0}", path.string());
			return false;
		}
		
		fout << out.c_str();
		fout.close();
		
		SetPath(path);
		ClearDirty();
		
		LNX_LOG_INFO("Animation clip saved: {0} ({1} channels, {2:.2f}s)", 
			path.string(), m_Channels.size(), m_Duration);
		return true;
	}
	
	Ref<AnimationClipAsset> AnimationClipAsset::LoadFromFile(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("AnimationClipAsset::LoadFromFile - File not found: {0}", path.string());
			return nullptr;
		}
		
		YAML::Node data;
		try {
			data = YAML::LoadFile(path.string());
		}
		catch (const YAML::Exception& e) {
			LNX_LOG_ERROR("AnimationClipAsset::LoadFromFile - YAML error: {0}", e.what());
			return nullptr;
		}
		
		auto clipNode = data["AnimationClip"];
		if (!clipNode) {
			LNX_LOG_ERROR("AnimationClipAsset::LoadFromFile - Invalid format: {0}", path.string());
			return nullptr;
		}
		
		auto clip = CreateRef<AnimationClipAsset>();
		clip->SetPath(path);
		
		if (clipNode["ID"])
			clip->SetID(UUID(clipNode["ID"].as<uint64_t>()));
		if (clipNode["Name"])
			clip->SetName(clipNode["Name"].as<std::string>());
		if (clipNode["Duration"])
			clip->m_Duration = clipNode["Duration"].as<float>();
		if (clipNode["TicksPerSecond"])
			clip->m_TicksPerSecond = clipNode["TicksPerSecond"].as<float>();
		if (clipNode["Loop"])
			clip->m_Loop = clipNode["Loop"].as<bool>();
		
		clip->DeserializeChannels(data["Channels"]);
		clip->SetLoaded(true);
		
		LNX_LOG_INFO("Animation clip loaded: {0} ({1} channels, {2:.2f}s)", 
			path.string(), clip->m_Channels.size(), clip->m_Duration);
		return clip;
	}
	
	void AnimationClipAsset::SerializeChannels(YAML::Emitter& out) const {
		out << YAML::Key << "Channels" << YAML::Value << YAML::BeginSeq;
		
		for (const auto& channel : m_Channels) {
			out << YAML::BeginMap;
			out << YAML::Key << "JointName" << YAML::Value << channel.JointName;
			out << YAML::Key << "KeyframeCount" << YAML::Value << channel.Keyframes.size();
			
			out << YAML::Key << "Keyframes" << YAML::Value << YAML::BeginSeq;
			for (const auto& kf : channel.Keyframes) {
				out << YAML::BeginMap;
				out << YAML::Key << "Time" << YAML::Value << kf.Time;
				out << YAML::Key << "Translation" << YAML::Value << YAML::Flow
					<< YAML::BeginSeq << kf.Translation.x << kf.Translation.y << kf.Translation.z << YAML::EndSeq;
				out << YAML::Key << "Rotation" << YAML::Value << YAML::Flow
					<< YAML::BeginSeq << kf.Rotation.w << kf.Rotation.x << kf.Rotation.y << kf.Rotation.z << YAML::EndSeq;
				out << YAML::Key << "Scale" << YAML::Value << YAML::Flow
					<< YAML::BeginSeq << kf.Scale.x << kf.Scale.y << kf.Scale.z << YAML::EndSeq;
				out << YAML::Key << "Interpolation" << YAML::Value << static_cast<int>(kf.Interpolation);
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
			
			out << YAML::EndMap;
		}
		
		out << YAML::EndSeq;
	}
	
	void AnimationClipAsset::DeserializeChannels(const YAML::Node& node) {
		if (!node || !node.IsSequence()) return;
		
		m_Channels.clear();
		m_Channels.reserve(node.size());
		
		for (const auto& channelNode : node) {
			AnimationChannel channel;
			
			if (channelNode["JointName"])
				channel.JointName = channelNode["JointName"].as<std::string>();
			
			auto keyframesNode = channelNode["Keyframes"];
			if (keyframesNode && keyframesNode.IsSequence()) {
				channel.Keyframes.reserve(keyframesNode.size());
				
				for (const auto& kfNode : keyframesNode) {
					AnimationKeyframe kf;
					
					if (kfNode["Time"])
						kf.Time = kfNode["Time"].as<float>();
					
					if (kfNode["Translation"]) {
						auto t = kfNode["Translation"];
						kf.Translation = glm::vec3(t[0].as<float>(), t[1].as<float>(), t[2].as<float>());
					}
					
					if (kfNode["Rotation"]) {
						auto r = kfNode["Rotation"];
						kf.Rotation = glm::quat(r[0].as<float>(), r[1].as<float>(), r[2].as<float>(), r[3].as<float>());
					}
					
					if (kfNode["Scale"]) {
						auto s = kfNode["Scale"];
						kf.Scale = glm::vec3(s[0].as<float>(), s[1].as<float>(), s[2].as<float>());
					}
					
					if (kfNode["Interpolation"])
						kf.Interpolation = static_cast<AnimationKeyframe::InterpolationType>(kfNode["Interpolation"].as<int>());
					
					channel.Keyframes.push_back(kf);
				}
			}
			
			m_Channels.push_back(channel);
		}
	}

}
