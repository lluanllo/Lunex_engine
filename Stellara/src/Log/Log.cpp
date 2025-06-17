#include "stpch.h"
#include "Log.h"

namespace Stellara {

	std::shared_ptr<spdlog::logger> Log::s_Logger;

    void Log::Init() {
        //cambio
        //chekea si hay alguyn logger con el nombre stellara
		//el codigo de antes no funcionaba porque no se creaba el logger si ya existia uno con el mismo nombre
        auto existingLogger = spdlog::get("STELLARA");
        if (existingLogger) {

            s_Logger = existingLogger;
            return;
        }

        spdlog::set_pattern("%^[%T] [%l] [%s:%#] %n: %v%$");

        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Stellara.log", true));


        s_Logger = std::make_shared<spdlog::logger>("STELLARA", begin(sinks), end(sinks));
        s_Logger->set_level(spdlog::level::trace);


        spdlog::register_logger(s_Logger);
    }
}