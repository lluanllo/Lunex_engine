#include "stpch.h"
#include "AnimationAsset.h"

#include "Log/Log.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <mutex>

namespace Lunex {

	AnimationAsset::AnimationAsset() : Asset("New Animation") {}
	AnimationAsset::AnimationAsset(const std::string& name) : Asset(name) {}

	AnimationBoneTrack* AnimationAsset::FindTrack(const std::string& bone) {
		for (auto& t : m_Tracks) if (t.Bone == bone) return &t;
		return nullptr;
	}

	const AnimationBoneTrack* AnimationAsset::FindTrack(const std::string& bone) const {
		for (const auto& t : m_Tracks) if (t.Bone == bone) return &t;
		return nullptr;
	}

	void AnimationAsset::SetKey(const std::string& bone, float time, AnimationKeyType type,
		const glm::vec3& position, const glm::quat& rotation)
	{
		AnimationBoneTrack* track = FindTrack(bone);
		if (!track) {
			m_Tracks.push_back({ bone, {} });
			track = &m_Tracks.back();
		}

		const float mergeDistance = 0.5f / std::max(FrameRate, 1.0f);
		for (auto& key : track->Keys) {
			if (std::abs(key.Time - time) < mergeDistance) {
				key.Type = static_cast<AnimationKeyType>(static_cast<uint8_t>(key.Type) | static_cast<uint8_t>(type));
				if ((static_cast<uint8_t>(type) & 2u) != 0) key.Position = position;
				if ((static_cast<uint8_t>(type) & 1u) != 0) key.Rotation = rotation;
				Touch();
				return;
			}
		}

		AnimationKeyframe key;
		key.Time = time;
		key.Type = type;
		key.Position = position;
		key.Rotation = rotation;
		track->Keys.push_back(key);
		std::sort(track->Keys.begin(), track->Keys.end(),
			[](const AnimationKeyframe& a, const AnimationKeyframe& b) { return a.Time < b.Time; });
		Duration = std::max(Duration, time);
		Touch();
	}

	bool AnimationAsset::RemoveKey(const std::string& bone, float time) {
		AnimationBoneTrack* track = FindTrack(bone);
		if (!track) return false;
		const float mergeDistance = 0.5f / std::max(FrameRate, 1.0f);
		auto it = std::find_if(track->Keys.begin(), track->Keys.end(),
			[&](const AnimationKeyframe& k) { return std::abs(k.Time - time) < mergeDistance; });
		if (it == track->Keys.end()) return false;
		track->Keys.erase(it);
		if (track->Keys.empty()) RemoveTrack(bone);
		Touch();
		return true;
	}

	void AnimationAsset::RemoveTrack(const std::string& bone) {
		m_Tracks.erase(std::remove_if(m_Tracks.begin(), m_Tracks.end(),
			[&](const AnimationBoneTrack& t) { return t.Bone == bone; }), m_Tracks.end());
		Touch();
	}

	size_t AnimationAsset::GetKeyCount() const {
		size_t n = 0;
		for (const auto& t : m_Tracks) n += t.Keys.size();
		return n;
	}

	const AnimationClip* AnimationAsset::GetClip(const Skeleton& skeleton) const {
		for (auto& cache : m_ClipCache) {
			if (cache.SkeletonPtr == &skeleton) {
				if (cache.Version == m_Version) return &cache.Clip;
				m_ClipCache.erase(std::find_if(m_ClipCache.begin(), m_ClipCache.end(),
					[&](const ClipCache& c) { return c.SkeletonPtr == &skeleton; }));
				break;
			}
		}

		ClipCache entry;
		entry.SkeletonPtr = &skeleton;
		entry.Version = m_Version;
		entry.Clip.Name = m_Name;
		entry.Clip.TicksPerSecond = 1.0f;   // key times are seconds
		entry.Clip.DurationTicks = std::max(Duration, 1e-3f);

		for (const auto& track : m_Tracks) {
			auto node = skeleton.NodeIndexByName.find(track.Bone);
			if (node == skeleton.NodeIndexByName.end()) continue;
			AnimationChannel ch;
			ch.NodeIndex = node->second;
			for (const auto& key : track.Keys) {
				if (key.HasPosition()) ch.Positions.push_back({ key.Time, key.Position });
				if (key.HasRotation()) ch.Rotations.push_back({ key.Time, key.Rotation });
			}
			entry.Clip.Channels.push_back(std::move(ch));
		}

		m_ClipCache.push_back(std::move(entry));
		return &m_ClipCache.back().Clip;
	}

	// ------------------------------------------------------------------ serialization

	static const char* KeyTypeToString(AnimationKeyType t) {
		switch (t) {
			case AnimationKeyType::Rotation: return "Rotation";
			case AnimationKeyType::Position: return "Position";
			default: return "Both";
		}
	}

	static AnimationKeyType KeyTypeFromString(const std::string& s) {
		if (s == "Rotation") return AnimationKeyType::Rotation;
		if (s == "Position") return AnimationKeyType::Position;
		return AnimationKeyType::Both;
	}

	bool AnimationAsset::SaveToFile(const std::filesystem::path& path) {
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Animation" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "ID" << YAML::Value << static_cast<uint64_t>(m_ID);
		out << YAML::Key << "Name" << YAML::Value << m_Name;
		out << YAML::Key << "Duration" << YAML::Value << Duration;
		out << YAML::Key << "FrameRate" << YAML::Value << FrameRate;
		out << YAML::Key << "Loop" << YAML::Value << Loop;
		out << YAML::EndMap;

		out << YAML::Key << "Tracks" << YAML::Value << YAML::BeginSeq;
		for (const auto& track : m_Tracks) {
			out << YAML::BeginMap;
			out << YAML::Key << "Bone" << YAML::Value << track.Bone;
			out << YAML::Key << "Keys" << YAML::Value << YAML::BeginSeq;
			for (const auto& key : track.Keys) {
				out << YAML::BeginMap;
				out << YAML::Key << "T" << YAML::Value << key.Time;
				out << YAML::Key << "Type" << YAML::Value << KeyTypeToString(key.Type);
				if (key.HasPosition())
					out << YAML::Key << "P" << YAML::Value << YAML::Flow << YAML::BeginSeq
						<< key.Position.x << key.Position.y << key.Position.z << YAML::EndSeq;
				if (key.HasRotation())
					out << YAML::Key << "R" << YAML::Value << YAML::Flow << YAML::BeginSeq
						<< key.Rotation.w << key.Rotation.x << key.Rotation.y << key.Rotation.z << YAML::EndSeq;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream fout(path);
		if (!fout) {
			LNX_LOG_ERROR("AnimationAsset: cannot write {0}", path.string());
			return false;
		}
		fout << out.c_str();
		m_FilePath = path;
		ClearDirty();
		Invalidate(path);
		return true;
	}

	Ref<AnimationAsset> AnimationAsset::LoadFromFile(const std::filesystem::path& path) {
		YAML::Node data;
		try {
			data = YAML::LoadFile(path.string());
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("AnimationAsset: failed to parse {0}: {1}", path.string(), e.what());
			return nullptr;
		}

		auto asset = CreateRef<AnimationAsset>(path.stem().string());
		asset->m_FilePath = path;
		if (auto header = data["Animation"]) {
			if (header["ID"]) asset->SetID(UUID(header["ID"].as<uint64_t>()));
			if (header["Name"]) asset->m_Name = header["Name"].as<std::string>();
			asset->Duration = header["Duration"].as<float>(2.0f);
			asset->FrameRate = header["FrameRate"].as<float>(30.0f);
			asset->Loop = header["Loop"].as<bool>(true);
		}

		if (auto tracks = data["Tracks"]; tracks && tracks.IsSequence()) {
			for (auto t : tracks) {
				AnimationBoneTrack track;
				track.Bone = t["Bone"].as<std::string>("");
				if (track.Bone.empty()) continue;
				for (auto k : t["Keys"]) {
					AnimationKeyframe key;
					key.Time = k["T"].as<float>(0.0f);
					key.Type = KeyTypeFromString(k["Type"].as<std::string>("Both"));
					if (auto p = k["P"]; p && p.size() == 3)
						key.Position = { p[0].as<float>(), p[1].as<float>(), p[2].as<float>() };
					if (auto r = k["R"]; r && r.size() == 4)
						key.Rotation = glm::quat(r[0].as<float>(), r[1].as<float>(), r[2].as<float>(), r[3].as<float>());
					track.Keys.push_back(key);
				}
				std::sort(track.Keys.begin(), track.Keys.end(),
					[](const AnimationKeyframe& a, const AnimationKeyframe& b) { return a.Time < b.Time; });
				asset->m_Tracks.push_back(std::move(track));
			}
		}

		asset->ClearDirty();
		asset->SetLoaded(true);
		return asset;
	}

	// ------------------------------------------------------------------ cache
	namespace {
		struct CacheEntry {
			Ref<AnimationAsset> Asset;
			std::filesystem::file_time_type Time{};
		};
		std::mutex s_CacheMutex;
		std::unordered_map<std::string, CacheEntry> s_Cache;

		std::string CacheKey(const std::filesystem::path& p) {
			std::error_code ec;
			auto c = std::filesystem::weakly_canonical(p, ec);
			std::string s = (ec ? p : c).lexically_normal().string();
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			return s;
		}
	}

	Ref<AnimationAsset> AnimationAsset::GetOrLoad(const std::filesystem::path& path) {
		std::error_code ec;
		auto time = std::filesystem::last_write_time(path, ec);
		if (ec) return nullptr;

		std::scoped_lock lock(s_CacheMutex);
		auto& entry = s_Cache[CacheKey(path)];
		// Keep in-memory edits (dirty) instead of reloading over them
		if (entry.Asset && (entry.Time == time || entry.Asset->IsDirty())) return entry.Asset;
		auto loaded = LoadFromFile(path);
		if (loaded) { entry.Asset = loaded; entry.Time = time; }
		return entry.Asset;
	}

	void AnimationAsset::Invalidate(const std::filesystem::path& path) {
		std::scoped_lock lock(s_CacheMutex);
		auto it = s_Cache.find(CacheKey(path));
		if (it != s_Cache.end()) {
			std::error_code ec;
			it->second.Time = std::filesystem::last_write_time(path, ec);
		}
	}

} // namespace Lunex
