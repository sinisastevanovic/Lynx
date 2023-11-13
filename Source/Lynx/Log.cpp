#include "Log.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Lynx
{
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init()
    {
        std::vector<spdlog::sink_ptr> coreSinks = {
            std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/Lynx.log", true),
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        };

        //coreSinks[0]->set_pattern("[%T] [%l] %n: %v");

        std::vector<spdlog::sink_ptr> clientSinks = {
            std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/Client.log", true),
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        };
        
        s_CoreLogger = std::make_shared<spdlog::logger>("LYNX", coreSinks.begin(), coreSinks.end());
        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->set_pattern("[%T] [%l] %n: %v");

        s_ClientLogger = std::make_shared<spdlog::logger>("APP", coreSinks.begin(), coreSinks.end());
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->set_pattern("[%T] [%l] %n: %v");
    }

    void Log::Shutdown()
    {
        s_CoreLogger.reset();
        s_ClientLogger.reset();
        spdlog::drop_all();
    }
}

