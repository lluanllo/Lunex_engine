#include "stpch.h"
#include "ResourceManager.h"
#include "Renderer/Texture.h"

namespace Lunex {
	std::unordered_map<std::string, Ref<Texture2D>> ResourceManager::s_Textures;
	
	void ResourceManager::Init() {
		// nothing for now
	}
	
	void ResourceManager::Shutdown() {
		s_Textures.clear();
	}
	
	void ResourceManager::RegisterTexture(const std::string& name, const Ref<Texture2D>& texture) {
		s_Textures[name] = texture;
	}
	
	Ref<Texture2D> ResourceManager::GetTexture(const std::string& name) {
		auto it = s_Textures.find(name);
		if (it == s_Textures.end()) return nullptr;
		return it->second;
	}
}
