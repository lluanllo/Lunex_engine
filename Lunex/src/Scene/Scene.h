#pragma once

#include "entt.hpp"

#include "Core/Timestep.h"

namespace Lunex {
	class LUNEX_API Entity;
	
	class LUNEX_API Scene {
		public:
			Scene();
			~Scene();
			
			Entity CreateEntity(const std::string& name = std::string());
			
			void OnUpdate(Timestep ts);
			
		private:
			entt::registry m_Registry;
			
			friend class Entity;
	};
}