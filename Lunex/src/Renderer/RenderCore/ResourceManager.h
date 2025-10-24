#pragma once

#include "Core/Core.h"
#include <string>
#include <unordered_map>

namespace Lunex {
	class ResourceManager {
		public:
			static void Init();
			static void Shutdown();
			
			static void RegisterTexture(const std::string& name, const Ref<class Texture2D>& texture);
			static Ref<class Texture2D> GetTexture(const std::string& name);
			
		private:
			static std::unordered_map<std::string, Ref<class Texture2D>> s_Textures;
	};
}
