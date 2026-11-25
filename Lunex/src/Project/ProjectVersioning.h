#pragma once

/**
 * @file ProjectVersioning.h
 * @brief Project version history (snapshots now, real VCS later).
 *
 * IVersionProvider is the seam for future backends (Git, Perforce, a Lunex server).
 * The built-in LocalSnapshotProvider stores full copies of the project content
 * (Assets + project file) under <project>/.lunex/versions/<id>/ and keeps a manifest
 * (versions.yaml) with id, message, author, date and size. Restoring first takes an
 * automatic safety snapshot, so nothing is ever lost.
 */

#include "Core/Core.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Lunex {

	struct ProjectVersion {
		std::string ID;              // e.g. "20261125-104512"
		std::string Message;
		std::string Author;
		int64_t Timestamp = 0;       // unix time
		uint64_t SizeBytes = 0;
		uint32_t FileCount = 0;
		bool Automatic = false;      // created by the editor (e.g. before a restore)
	};

	class IVersionProvider {
	public:
		virtual ~IVersionProvider() = default;
		virtual const char* GetName() const = 0;

		virtual std::vector<ProjectVersion> List() const = 0;
		virtual bool Commit(const std::string& message, bool automatic = false, ProjectVersion* outVersion = nullptr) = 0;
		virtual bool Restore(const std::string& versionID) = 0;
		virtual bool Delete(const std::string& versionID) = 0;
	};

	class LocalSnapshotProvider : public IVersionProvider {
	public:
		explicit LocalSnapshotProvider(const std::filesystem::path& projectFile);

		const char* GetName() const override { return "Local Snapshots"; }
		std::vector<ProjectVersion> List() const override;
		bool Commit(const std::string& message, bool automatic = false, ProjectVersion* outVersion = nullptr) override;
		bool Restore(const std::string& versionID) override;
		bool Delete(const std::string& versionID) override;

		std::filesystem::path GetVersionsDirectory() const { return m_ProjectDir / ".lunex" / "versions"; }

	private:
		std::vector<ProjectVersion> LoadManifest() const;
		bool SaveManifest(const std::vector<ProjectVersion>& versions) const;
		std::vector<std::filesystem::path> GetTrackedRoots() const;   // what a snapshot contains

		std::filesystem::path m_ProjectFile;
		std::filesystem::path m_ProjectDir;
	};

	namespace ProjectVersioning {
		// Provider for a project (only local snapshots exist for now)
		Scope<IVersionProvider> CreateProvider(const std::filesystem::path& projectFile);
	}

} // namespace Lunex
