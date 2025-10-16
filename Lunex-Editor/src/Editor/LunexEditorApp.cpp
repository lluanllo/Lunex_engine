#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Lunex {
	class LunexEditor : public Application{
		public:
			LunexEditor(ApplicationCommandLineArgs args)
				: Application("Lunex Editor", args) {
				PushLayer(new EditorLayer());
			}
			
			~LunexEditor() {
			}
	};
	
	Lunex::Ref <Lunex::Application> Lunex::CreateApplication(ApplicationCommandLineArgs args) {
		return Lunex::CreateScope<LunexEditor>(args);
	}
}