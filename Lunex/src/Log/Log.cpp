#include "stpch.h"
#include "Log.h"

namespace Lunex {

    std::shared_ptr<spdlog::logger> Log::s_Logger;

    void Log::Init() {
        spdlog::set_pattern("%^[%T] [%l] [%s:%#] %n: %v%$");
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Lunex.log", true));
        s_Logger = std::make_shared<spdlog::logger>("LUNEX", begin(sinks), end(sinks));
        s_Logger->set_level(spdlog::level::trace);
        spdlog::register_logger(s_Logger);
    }
}