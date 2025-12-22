#pragma once

/**
 * @file AnimationClipAsset.h
 * @brief Animation clip asset for skeletal animation
 * 
 * Part of the AAA Animation System
 * 
 * An animation clip defines the movement of bones over time.
 * It contains:
 *   - Keyframes for each animated bone
 *   - Duration and timing information
 *   - Loop and playback settings
 */

#include "Assets/Core/Asset.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>

namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex {

	// ============================================================================
	// KEYFRAME
	// ============================================================================
	
	/**
	 * @struct AnimationKeyframe
	 * @brief A single keyframe in an animation
	 */
	struct AnimationKeyframe {
		float Time = 0.0f;                                           // Time in seconds
		glm::vec3 Translation = glm::vec3(0.0f);                     // Position
		glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);      // Rotation
		glm::vec3 Scale = glm::vec3(1.0f);                           // Scale
		
		// Interpolation type
		enum class InterpolationType : uint8_t {
			Step,       // No interpolation
			Linear,     // Linear interpolation
			Cubic       // Cubic spline interpolation
		};
		InterpolationType Interpolation = InterpolationType::Linear;
	};

	// ============================================================================
	// ANIMATION CHANNEL
	// ============================================================================
	
	/**
	 * @struct AnimationChannel
	 * @brief Keyframes for a single bone
	 */
	struct AnimationChannel {
		std::string JointName;              // Name of the target joint
		int32_t JointIndex = -1;            // Index in skeleton (resolved at runtime)
		
		std::vector<AnimationKeyframe> Keyframes;
		
		// Get keyframe at time (with interpolation)
		AnimationKeyframe Sample(float time) const;
		
		// Get duration of this channel
		float GetDuration() const;
		
		// Sort keyframes by time
		void SortKeyframes();
	};

	// ============================================================================
	// ANIMATION CLIP ASSET (.luanim)
	// ============================================================================
	
	/**
	 * @class AnimationClipAsset
	 * @brief A single animation clip (walk, run, idle, etc.)
	 * 
	 * Usage:
	 *   1. Import from model file (glTF, FBX)
	 *   2. Save to .luanim format
	 *   3. Assign to AnimatorComponent
	 */
	class AnimationClipAsset : public Asset {
	public:
		AnimationClipAsset();
		AnimationClipAsset(const std::string& name);
		~AnimationClipAsset() = default;

		// Asset interface
		AssetType GetType() const override { return AssetType::Animation; }
		static AssetType StaticType() { return AssetType::Animation; }
		const char* GetExtension() const override { return ".luanim"; }

		// ========== CLIP PROPERTIES ==========
		
		// Duration in seconds
		float GetDuration() const { return m_Duration; }
		void SetDuration(float duration) { m_Duration = duration; MarkDirty(); }
		
		// Ticks per second (for import conversion)
		float GetTicksPerSecond() const { return m_TicksPerSecond; }
		void SetTicksPerSecond(float tps) { m_TicksPerSecond = tps; MarkDirty(); }
		
		// Loop mode
		bool IsLooping() const { return m_Loop; }
		void SetLooping(bool loop) { m_Loop = loop; MarkDirty(); }
		
		// ========== CHANNEL MANAGEMENT ==========
		
		// Add a channel
		void AddChannel(const AnimationChannel& channel);
		
		// Get channel by joint name
		const AnimationChannel* GetChannel(const std::string& jointName) const;
		AnimationChannel* GetChannel(const std::string& jointName);
		
		// Get channel by index
		const AnimationChannel& GetChannel(size_t index) const;
		AnimationChannel& GetChannel(size_t index);
		
		// Get all channels
		const std::vector<AnimationChannel>& GetChannels() const { return m_Channels; }
		std::vector<AnimationChannel>& GetChannels() { return m_Channels; }
		
		// Channel count
		size_t GetChannelCount() const { return m_Channels.size(); }
		
		// ========== SAMPLING ==========
		
		/**
		 * Sample the animation at a specific time
		 * @param time Time in seconds
		 * @param outPose Output pose (translation, rotation, scale per joint)
		 * @note Joint indices must be resolved before sampling
		 */
		void Sample(float time, std::vector<AnimationKeyframe>& outPose) const;
		
		// Resolve joint indices against a skeleton
		void ResolveJointIndices(const class SkeletonAsset& skeleton);
		
		// ========== UTILITY ==========
		
		// Compute duration from channels
		void ComputeDuration();
		
		// Normalize time (handle looping)
		float NormalizeTime(float time) const;
		
		// ========== SERIALIZATION ==========
		
		bool SaveToFile(const std::filesystem::path& path) override;
		static Ref<AnimationClipAsset> LoadFromFile(const std::filesystem::path& path);

	private:
		void SerializeChannels(YAML::Emitter& out) const;
		void DeserializeChannels(const YAML::Node& node);

	private:
		std::vector<AnimationChannel> m_Channels;
		
		float m_Duration = 0.0f;          // Total duration in seconds
		float m_TicksPerSecond = 30.0f;   // Original ticks per second
		bool m_Loop = true;               // Loop by default
	};

}
