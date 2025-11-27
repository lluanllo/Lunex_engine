#include <Lunex.h>
#include <Core/EntryPoint.h>

#include "LauncherLayer.h"

namespace Lunex {

	class LunexLauncher : public Application {
	public:
		LunexLauncher(ApplicationCommandLineArgs args)
			: Application("Lunex Launcher", args)
		{
			PushLayer(new LauncherLayer());
		}

		~LunexLauncher()
		{
		}
	};

	Ref<Application> CreateApplication(ApplicationCommandLineArgs args)
	{
		return CreateScope<LunexLauncher>(args);
	}

}
