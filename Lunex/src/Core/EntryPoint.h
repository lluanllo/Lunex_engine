#pragma once

#include "stpch.h"
#include "Application.h"

#ifdef LN_PLATFORM_WINDOWS

	extern Lunex::Application* Lunex::CreateApplication();

	int main(int argc, char** argv){
		Lunex::Log::Init();
		LNX_LOG_WARN("Initialized Log!");

		auto app = Lunex::CreateApplication();
		app->Run();
		delete app;
	}

#endif