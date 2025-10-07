#pragma once

#include "Core/layer.h"

#include "Events/Event.h"
#include "Events/ApplicationEvent.h"
#include "Events/MouseEvent.h"
#include "Events/KeyEvent.h"

namespace Lunex {

	class LUNEX_API ImGuiLayer : public Layer {
		public:
			ImGuiLayer();
			~ImGuiLayer();
			
			virtual void OnDetach() override;
			virtual void OnAttach() override;
			virtual void OnEvent(Event& e) override;
			
			void Begin();
			void End();
			
			void BlockEvents(bool block) { m_BlockEvents = block; }
			
		private:		
			bool m_BlockEvents = true;
			float m_Time = 0.0f;
	};

}