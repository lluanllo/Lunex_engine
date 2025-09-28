#pragma once

#include "stpch.h"
#include "Application.h"

#ifdef LN_PLATFORM_WINDOWS
	
	extern Lunex::Ref<Lunex::Application> Lunex::CreateApplication();
	
	int main(int argc, char** argv) {
		Lunex::Log::Init();
		
		LNX_PROFILE_BEGIN_SESSION("Startup", "LunexProfile-Startup.json");
		auto app = Lunex::CreateApplication();
		LNX_PROFILE_END_SESSION();
		
		LNX_PROFILE_BEGIN_SESSION("Runtime", "LunexProfile-Runtime.json");
		app->Run();
		LNX_PROFILE_END_SESSION();
		LNX_PROFILE_BEGIN_SESSION("Startup", "LunexProfile-Shutdown.json");
		LNX_PROFILE_END_SESSION();
	}
#endif