#include "stpch.h"
#include "AssetCore.h"
#include "Log/Log.h"

namespace Lunex {

	void AssetManager::Initialize() {
		AssetRegistry::Get().Initialize();
		AssetLoader::Get().Initialize();
		
		LNX_LOG_INFO("AssetManager initialized (Unified Asset System)");
	}

	void AssetManager::Shutdown() {
		AssetLoader::Get().Shutdown();
		AssetRegistry::Get().Shutdown();
		
		LNX_LOG_INFO("AssetManager shutdown");
	}

} // namespace Lunex
