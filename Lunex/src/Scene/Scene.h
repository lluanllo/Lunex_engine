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
			void OnViewportResize(uint32_t width, uint32_t height);
			
		private:
			entt::registry m_Registry;
			uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
			
			friend class Entity;
	};
}