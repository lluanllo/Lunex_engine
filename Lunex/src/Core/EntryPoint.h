#pragma once

#include "Core/Core.h"
#include "Application.h"

#ifdef LNX_PLATFORM_WINDOWS
	
	extern Lunex::Ref<Lunex::Application> Lunex::CreateApplication(ApplicationCommandLineArgs args);
	
	int main(int argc, char** argv) {
		Lunex::Log::Init();
		
		LNX_PROFILE_BEGIN_SESSION("Startup", "LunexProfile-Startup.json");
		auto app = Lunex::CreateApplication({argc, argv});
		LNX_PROFILE_END_SESSION();
		
		LNX_PROFILE_BEGIN_SESSION("Runtime", "LunexProfile-Runtime.json");
		app->Run();
		LNX_PROFILE_END_SESSION();
		
		// Explicitly destroy the application before static destructors run.
		// This prevents use-after-free crashes caused by MSVC's destructor tombstones
		// when shared_ptrs in static storage are accessed after being destroyed.
		app.reset();
		
		// Shutdown logging system: clears static shared_ptrs and shuts down spdlog
		// before C++ runtime static destructors run.
		Lunex::Log::Shutdown();
		
		return 0;
	}
#endif