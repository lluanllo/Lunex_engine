#include "stpch.h"
#include "Project.h"

namespace Lunex {
	std::filesystem::path Project::GetAssetFileSystemPath(const std::filesystem::path& path) const {
		return m_AssetDirectory / path;
	}
	
	std::filesystem::path Project::GetAssetRelativePath(const std::filesystem::path& path) const {
		return std::filesystem::relative(path, m_AssetDirectory);
	}
}
