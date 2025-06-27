#pragma once

#include "layer.h"
#include "imgui.h"

#include "Events/Event.h"
#include "Events/ApplicationEvent.h"
#include "Events/MouseEvent.h"
#include "Events/KeyEvent.h"

namespace Stellara {

	class STELLARA_API ImGuiLayer : public Layer {
		public:
			ImGuiLayer();
			~ImGuiLayer();
		
			virtual void OnDetach() override;
			virtual void OnAttach() override;
			virtual void OnImGuiRender() override;
		
			void Begin();
			void End();
		
		private:
			float m_Time = 0.0f;
	};

}