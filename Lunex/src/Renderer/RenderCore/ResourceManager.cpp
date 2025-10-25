#include "stpch.h"
#include "ResourceManager.h"
#include "Renderer/Texture.h"

namespace Lunex {
	std::unordered_map<std::string, Ref<Texture2D>> ResourceManager::s_Textures;
	
	void ResourceManager::Init() {
		LNX_LOG_INFO("ResourceManager initialized");
	}
	
	void ResourceManager::Shutdown() {
		s_Textures.clear();
		LNX_LOG_INFO("ResourceManager shut down");
	}
	
	void ResourceManager::RegisterTexture(const std::string& name, const Ref<Texture2D>& texture) {
		s_Textures[name] = texture;
	}
	
    Ref<Texture2D> ResourceManager::GetTexture(const std::string& name) {
        auto it = s_Textures.find(name);
        if (it != s_Textures.end())
            return it->second;
            
        LNX_LOG_WARN("Texture '{}' not found!", name);
        return nullptr;
    }
        
    Ref<Texture2D> ResourceManager::LoadTexture(const std::string& path, const std::string& name) {
        std::string texName = name.empty() ? path : name;
            
        auto it = s_Textures.find(texName);
        if (it != s_Textures.end())
            return it->second;
            
        auto texture = Texture2D::Create(path);
        if (texture && texture->IsLoaded()) {
            s_Textures[texName] = texture;
            LNX_LOG_INFO("Loaded texture '{}'", texName);
            return texture;
        }
            
        LNX_LOG_ERROR("Failed to load texture: {}", path);
        return nullptr;
    }
}
