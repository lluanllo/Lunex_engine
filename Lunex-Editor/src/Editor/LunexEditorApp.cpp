#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Lunex {
	class LunexEditor : public Application {
	public:
		LunexEditor(ApplicationCommandLineArgs args)
			: Application("Lunex Editor", args) {
			PushLayer(new EditorLayer());
		}
		
		~LunexEditor() {
		}
	};

	// CreateApplication debe estar dentro del namespace Lunex
	Ref<Application> CreateApplication(ApplicationCommandLineArgs args) {
		return CreateRef<LunexEditor>(args);
	}
}