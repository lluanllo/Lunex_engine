#pragma once

#include "Events/Event.h"

#include <iostream>

namespace Stellara {

	class Layer {
		public:
			Layer(const std::string& name = "Layer");
			virtual ~Layer() = default;

			virtual void OnAttach() {}
			virtual void OnDetach() {}
			virtual void OnUpdate(float deltaTime) {}
			virtual void OnEvent(Event& e) {}

			inline const std::string& GetName() const { return m_DebugName; }
		protected:
			std::string m_DebugName;
	};
}