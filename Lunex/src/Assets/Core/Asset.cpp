#include "stpch.h"
#include "Asset.h"

#include "Log/Log.h"

namespace Lunex {

	Asset::Asset() {
		GenerateID();
	}

	Asset::Asset(const std::string& name)
		: m_Name(name) {
		GenerateID();
	}

	void Asset::GenerateID() {
		m_ID = UUID();
	}

	bool Asset::Save() {
		if (m_FilePath.empty()) {
			LNX_LOG_ERROR("Asset::Save - No file path set for asset: {0}", m_Name);
			return false;
		}
		return SaveToFile(m_FilePath);
	}

	AssetMetadata Asset::GetMetadata() const {
		AssetMetadata metadata;
		metadata.ID = m_ID;
		metadata.Type = GetType();
		metadata.FilePath = m_FilePath;
		metadata.Name = m_Name;
		metadata.IsLoaded = IsLoaded();
		metadata.SourcePath = m_SourcePath;
		
		if (!m_FilePath.empty() && std::filesystem::exists(m_FilePath)) {
			metadata.LastModified = std::filesystem::last_write_time(m_FilePath);
		}
		
		return metadata;
	}

}
