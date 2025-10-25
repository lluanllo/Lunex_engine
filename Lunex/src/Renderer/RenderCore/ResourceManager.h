#pragma once

#include "Core/Core.h"

#include "Renderer/Texture.h"

#include <string>
#include <unordered_map>

namespace Lunex {
	class ResourceManager {
		public:
            static void Init();
            static void Shutdown();
                
            // --- TEXTURES ---
            static void RegisterTexture(const std::string& name, const Ref<Texture2D>& texture);
            static Ref<Texture2D> GetTexture(const std::string& name);
            static Ref<Texture2D> LoadTexture(const std::string& path, const std::string& name = "");
                
            // --- SHADERS (a futuro) ---
            // static void RegisterShader(const std::string& name, const Ref<Shader>& shader);
            // static Ref<Shader> GetShader(const std::string& name);
			
		private:
			static std::unordered_map<std::string, Ref<class Texture2D>> s_Textures;
	};
}
