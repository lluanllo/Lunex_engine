#pragma once

/**
 * @file AnimationAsset.h
 * @brief Authored skeletal animation (.luanim), editable in the Animation Editor.
 *
 * Tracks are keyed by bone NAME, so a clip works on any mesh whose skeleton uses the
 * same bone names (e.g. every Mixamo rig). Each keyframe stores rotation, position or
 * both; missing channels fall back to the bind pose when sampling.
 */

#include "Assets/Core/Asset.h"
#include "Resources/Animation/Animation.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace Lunex {

	enum class AnimationKeyType : uint8_t {
		Rotation = 1,
		Position = 2,
		Both = 3
	};

	struct AnimationKeyframe {
		float Time = 0.0f;                       // seconds
		AnimationKeyType Type = AnimationKeyType::Rotation;
		glm::vec3 Position{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

		bool HasRotation() const { return (static_cast<uint8_t>(Type) & 1u) != 0; }
		bool HasPosition() const { return (static_cast<uint8_t>(Type) & 2u) != 0; }
	};

	struct AnimationBoneTrack {
		std::string Bone;
		std::vector<AnimationKeyframe> Keys;   // sorted by time
	};

	class AnimationAsset : public Asset {
	public:
		AnimationAsset();
		explicit AnimationAsset(const std::string& name);

		AssetType GetType() const override { return AssetType::Animation; }
		static AssetType StaticType() { return AssetType::Animation; }

		// ---- Settings ----
		float Duration = 2.0f;     // seconds
		float FrameRate = 30.0f;   // keyframe snapping in the editor
		bool Loop = true;

		// ---- Tracks ----
		std::vector<AnimationBoneTrack>& GetTracks() { return m_Tracks; }
		const std::vector<AnimationBoneTrack>& GetTracks() const { return m_Tracks; }
		AnimationBoneTrack* FindTrack(const std::string& bone);
		const AnimationBoneTrack* FindTrack(const std::string& bone) const;

		// Insert or update a key. Keys closer than half a frame merge (types are OR-ed).
		void SetKey(const std::string& bone, float time, AnimationKeyType type,
			const glm::vec3& position, const glm::quat& rotation);
		bool RemoveKey(const std::string& bone, float time);
		void RemoveTrack(const std::string& bone);
		size_t GetKeyCount() const;

		// Runtime clip for a skeleton (cached until the asset is edited)
		const AnimationClip* GetClip(const Skeleton& skeleton) const;

		// ---- Serialization ----
		bool SaveToFile(const std::filesystem::path& path) override;
		static Ref<AnimationAsset> LoadFromFile(const std::filesystem::path& path);

		// Shared cache (reloads automatically when the file changes on disk)
		static Ref<AnimationAsset> GetOrLoad(const std::filesystem::path& path);
		static void Invalidate(const std::filesystem::path& path);

		uint64_t GetVersion() const { return m_Version; }

		// Call after editing GetTracks() directly (retiming / changing key types)
		void MarkEdited() { Touch(); }

	private:
		void Touch() { m_Version++; MarkDirty(); }

		std::vector<AnimationBoneTrack> m_Tracks;
		uint64_t m_Version = 1;

		struct ClipCache {
			const Skeleton* SkeletonPtr = nullptr;
			uint64_t Version = 0;
			AnimationClip Clip;
		};
		mutable std::vector<ClipCache> m_ClipCache;
	};

} // namespace Lunex
