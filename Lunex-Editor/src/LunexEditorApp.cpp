#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Lunex {
	class LunexEditor : public Application{
		public:
			LunexEditor() {
				PushLayer(new EditorLayer());
			}
			
			~LunexEditor() {
			}
	};
	
	Lunex::Ref <Lunex::Application> Lunex::CreateApplication() {
		return Lunex::CreateScope<LunexEditor>();
	}
}