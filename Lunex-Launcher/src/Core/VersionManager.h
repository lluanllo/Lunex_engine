#pragma once

#include <string>
#include <vector>

namespace Lunex {

	struct Version {
		int Major = 1;
		int Minor = 0;
		int Patch = 0;
		std::string BuildType = "Dev"; // Dev, Alpha, Beta, Release
		
		std::string ToString() const {
			return std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch) + " " + BuildType;
		}
		
		bool operator>(const Version& other) const {
			if (Major != other.Major) return Major > other.Major;
			if (Minor != other.Minor) return Minor > other.Minor;
			return Patch > other.Patch;
		}
		
		bool operator==(const Version& other) const {
			return Major == other.Major && Minor == other.Minor && Patch == other.Patch;
		}
		
		bool operator<(const Version& other) const {
			return !(*this > other) && !(*this == other);
		}
	};

	class VersionManager {
	public:
		static void Init();
		static Version GetCurrentVersion() { return s_CurrentVersion; }
		static Version GetLatestVersion();
		
		// Check if update is available
		static bool IsUpdateAvailable();
		
		// Check prerequisites (Vulkan, OpenGL, etc.)
		static bool CheckPrerequisites();
		static std::vector<std::string> GetMissingPrerequisites();
		
	private:
		static Version s_CurrentVersion;
		static Version s_LatestVersion;
	};

}
