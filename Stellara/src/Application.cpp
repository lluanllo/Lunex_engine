#include "Core/Stellara.hpp"

Stellara::Application::Application() {
	Stellara::Log::Init();
	STLR_LOG_INFO("Logger Initialized");
	STLR_LOG_DEBUG("Stellara Application Initialized");
}

Stellara::Application::~Application() {
	// Cleanup code here
}

void Stellara::Application::Run() {
	while (true){

	}
}