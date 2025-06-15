#include "stpch.h"
#include "Core/Stellara.hpp"

int main(int argc, char** argv) {
	
	Stellara::Application app;
	app.~Application();
	app.Run();
	return 0;
}