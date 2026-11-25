#include "stpch.h"
#include "ProjectVersioning.h"

#include "Log/Log.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>

namespace Lunex {

	namespace fs = std::filesystem;

	namespace {
		std::string MakeVersionID(int64_t t) {
			std::tm tm{};
			std::time_t tt = static_cast<std::time_t>(t);
			localtime_s(&tm, &tt);
			char buf[32];
			std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
			return buf;
		}

		std::string CurrentUser() {
			char* user = nullptr;
			size_t len = 0;
			if (_dupenv_s(&user, &len, "USERNAME") == 0 && user) {
				std::string s(user);
				free(user);
				return s;
			}
			return "unknown";
		}

		// Copy a file or directory tree; returns files copied and bytes
		void CopyTree(const fs::path& from, const fs::path& to, uint32_t& files, uint64_t& bytes) {
			std::error_code ec;
			if (fs::is_regular_file(from, ec)) {
				fs::create_directories(to.parent_path(), ec);
				fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
				if (!ec) { files++; bytes += fs::file_size(from, ec); }
				return;
			}
			for (auto it = fs::recursive_directory_iterator(from, fs::directory_options::skip_permission_denied, ec);
				it != fs::recursive_directory_iterator(); it.increment(ec))
			{
				if (ec) break;
				const fs::path rel = fs::relative(it->path(), from, ec);
				// Skip generated caches; they are rebuilt on demand
				if (!rel.empty() && (*rel.begin() == ".cache" || *rel.begin() == "Cache")) { it.disable_recursion_pending(); continue; }
				const fs::path dst = to / rel;
				if (it->is_directory(ec)) { fs::create_directories(dst, ec); continue; }
				if (!it->is_regular_file(ec)) continue;
				fs::create_directories(dst.parent_path(), ec);
				fs::copy_file(it->path(), dst, fs::copy_options::overwrite_existing, ec);
				if (!ec) { files++; bytes += it->file_size(ec); }
			}
		}
	}

	LocalSnapshotProvider::LocalSnapshotProvider(const fs::path& projectFile)
		: m_ProjectFile(projectFile), m_ProjectDir(projectFile.parent_path()) {}

	std::vector<fs::path> LocalSnapshotProvider::GetTrackedRoots() const {
		std::vector<fs::path> roots;
		roots.push_back(m_ProjectFile.filename());
		// Every top-level folder except hidden/build output (Assets, Scripts, ...)
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(m_ProjectDir, ec)) {
			if (!entry.is_directory(ec)) continue;
			std::string name = entry.path().filename().string();
			if (name.empty() || name[0] == '.' || name == "bin" || name == "bin-int" || name == "Binaries" || name == "Intermediate")
				continue;
			roots.push_back(entry.path().filename());
		}
		return roots;
	}

	std::vector<ProjectVersion> LocalSnapshotProvider::LoadManifest() const {
		std::vector<ProjectVersion> versions;
		fs::path manifest = GetVersionsDirectory() / "versions.yaml";
		if (!fs::exists(manifest)) return versions;
		try {
			YAML::Node data = YAML::LoadFile(manifest.string());
			for (const auto& v : data["Versions"]) {
				ProjectVersion pv;
				pv.ID = v["ID"].as<std::string>("");
				pv.Message = v["Message"].as<std::string>("");
				pv.Author = v["Author"].as<std::string>("");
				pv.Timestamp = v["Time"].as<int64_t>(0);
				pv.SizeBytes = v["Size"].as<uint64_t>(0);
				pv.FileCount = v["Files"].as<uint32_t>(0);
				pv.Automatic = v["Automatic"].as<bool>(false);
				if (!pv.ID.empty() && fs::exists(GetVersionsDirectory() / pv.ID)) versions.push_back(pv);
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_WARN("Version manifest unreadable ({0}): {1}", manifest.string(), e.what());
		}
		return versions;
	}

	bool LocalSnapshotProvider::SaveManifest(const std::vector<ProjectVersion>& versions) const {
		YAML::Emitter out;
		out << YAML::BeginMap << YAML::Key << "Versions" << YAML::Value << YAML::BeginSeq;
		for (const auto& v : versions) {
			out << YAML::BeginMap;
			out << YAML::Key << "ID" << YAML::Value << v.ID;
			out << YAML::Key << "Message" << YAML::Value << v.Message;
			out << YAML::Key << "Author" << YAML::Value << v.Author;
			out << YAML::Key << "Time" << YAML::Value << v.Timestamp;
			out << YAML::Key << "Size" << YAML::Value << v.SizeBytes;
			out << YAML::Key << "Files" << YAML::Value << v.FileCount;
			out << YAML::Key << "Automatic" << YAML::Value << v.Automatic;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq << YAML::EndMap;

		std::error_code ec;
		fs::create_directories(GetVersionsDirectory(), ec);
		std::ofstream f(GetVersionsDirectory() / "versions.yaml", std::ios::trunc);
		if (!f) return false;
		f << out.c_str();
		return true;
	}

	std::vector<ProjectVersion> LocalSnapshotProvider::List() const {
		auto versions = LoadManifest();
		std::sort(versions.begin(), versions.end(), [](const auto& a, const auto& b) { return a.Timestamp > b.Timestamp; });
		return versions;
	}

	bool LocalSnapshotProvider::Commit(const std::string& message, bool automatic, ProjectVersion* outVersion) {
		if (!fs::exists(m_ProjectFile)) return false;

		ProjectVersion v;
		v.Timestamp = static_cast<int64_t>(std::time(nullptr));
		v.ID = MakeVersionID(v.Timestamp);
		auto versions = LoadManifest();
		// Unique id when several snapshots happen within the same second
		for (int suffix = 1; std::any_of(versions.begin(), versions.end(), [&](const auto& e) { return e.ID == v.ID; }); suffix++)
			v.ID = MakeVersionID(v.Timestamp) + "-" + std::to_string(suffix);
		v.Message = message.empty() ? std::string("Snapshot") : message;
		v.Author = CurrentUser();
		v.Automatic = automatic;

		const fs::path dst = GetVersionsDirectory() / v.ID;
		for (const auto& root : GetTrackedRoots())
			CopyTree(m_ProjectDir / root, dst / root, v.FileCount, v.SizeBytes);

		versions.push_back(v);
		if (!SaveManifest(versions)) return false;
		if (outVersion) *outVersion = v;
		LNX_LOG_INFO("Project snapshot '{0}' created ({1} files)", v.Message, v.FileCount);
		return true;
	}

	bool LocalSnapshotProvider::Restore(const std::string& versionID) {
		const fs::path src = GetVersionsDirectory() / versionID;
		if (!fs::exists(src)) return false;

		// Safety net: snapshot the current state first
		Commit("Before restoring " + versionID, true);

		// Replace tracked content with the snapshot's
		std::error_code ec;
		for (const auto& root : GetTrackedRoots()) {
			if (root == m_ProjectFile.filename()) continue;
			fs::remove_all(m_ProjectDir / root, ec);
		}
		uint32_t files = 0;
		uint64_t bytes = 0;
		for (const auto& entry : fs::directory_iterator(src, ec))
			CopyTree(entry.path(), m_ProjectDir / entry.path().filename(), files, bytes);
		LNX_LOG_INFO("Project restored to {0} ({1} files)", versionID, files);
		return true;
	}

	bool LocalSnapshotProvider::Delete(const std::string& versionID) {
		auto versions = LoadManifest();
		auto it = std::find_if(versions.begin(), versions.end(), [&](const auto& v) { return v.ID == versionID; });
		if (it == versions.end()) return false;
		std::error_code ec;
		fs::remove_all(GetVersionsDirectory() / versionID, ec);
		versions.erase(it);
		return SaveManifest(versions);
	}

	Scope<IVersionProvider> ProjectVersioning::CreateProvider(const fs::path& projectFile) {
		return CreateScope<LocalSnapshotProvider>(projectFile);
	}

} // namespace Lunex
