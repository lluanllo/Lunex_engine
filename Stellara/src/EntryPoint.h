#pragma once

#include "stpch.h"
#include "Application.h"

#ifdef ST_PLATFORM_WINDOWS

extern Stellara::Application* Stellara::CreateApplication();

int main(int argc, char** argv)
{
	Stellara::Log::Init();
	STLR_LOG_WARN("Initialized Log!");

	auto app = Stellara::CreateApplication();
	app->Run();
	delete app;
}

#endif