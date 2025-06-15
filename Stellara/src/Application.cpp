#include "stpch.h"
#include "Application.h"
#include "Events/ApplicationEvent.h"
#include "Log/Log.h"

namespace Stellara{

	Stellara::Application::Application() {
		Stellara::Log::Init();
		STLR_LOG_INFO("Logger Initialized");
		STLR_LOG_DEBUG("Stellara Application Initialized");
	}

	Stellara::Application::~Application() {
		// Cleanup code here
	}

	void Stellara::Application::Run() {
		WindowResizeEvent resizeEvent(1280, 720);
		STLR_LOG_INFO("Window Resize Event: {0}", resizeEvent.ToString());
		while (true){

		}
	}

}