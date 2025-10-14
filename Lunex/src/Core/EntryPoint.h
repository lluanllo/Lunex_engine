#pragma once

#include "Application.h"

#ifdef LN_PLATFORM_WINDOWS
	
	extern Lunex::Ref<Lunex::Application> Lunex::CreateApplication(ApplicationCommandLineArgs args);
	
	int main(int argc, char** argv) {
		Lunex::Log::Init();
		
		LNX_PROFILE_BEGIN_SESSION("Startup", "LunexProfile-Startup.json");
		auto app = Lunex::CreateApplication({argc, argv});
		LNX_PROFILE_END_SESSION();
		
		LNX_PROFILE_BEGIN_SESSION("Runtime", "LunexProfile-Runtime.json");
		app->Run();
		LNX_PROFILE_END_SESSION();
	}
#endif