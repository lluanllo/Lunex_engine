#pragma once

#include "layer.h"

#include "Events/Event.h"
#include "Events/MouseEvent.h"
#include "Events/KeyEvent.h"

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
			void OnMouseButtonPressedEvent(MouseButtonPressedEvent& e);
			void OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e);
			void OnMouseMovedEvent(MouseMovedEvent& e);
			void OnMouseScrolledEvent(MouseScrolledEvent& e);
			void OnKeyPressedEvent(KeyPressedEvent& e);
			void OnKeyReleasedEvent(KeyReleasedEvent& e);
			//void OnKeyTypedEvent(KeyTypedEvent& e);
			void OnWindowResizeEvent(WindowResizeEvent& e);
		private:
			float m_Time = 0.0f;

	};

}