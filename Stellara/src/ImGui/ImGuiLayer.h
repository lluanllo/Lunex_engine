#pragma once

#include "../Core/Core.h"
#include "../layer.h"

namespace Stellara {

	class STELLARA_API ImGuiLayer : public Layer {
		public:
			ImGuiLayer();
			~ImGuiLayer();
		
			void OnAttach();
			void OnDetach();
			void OnUpdate();
			void OnEvent(Event& event); 

		private:
			float m_Time = 0.0f;
	};

}