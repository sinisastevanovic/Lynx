#include "lxpch.h"

#include <filesystem>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "spdlog/sinks/ansicolor_sink.h"
#include "SessionFileSink.h"

namespace Lynx
{
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init(const std::string& appName)
    {
        if (!std::filesystem::exists("logs"))
            std::filesystem::create_directories("logs");
        
        std::string fileName = "logs/" + appName + ".log";
        
        std::vector<spdlog::sink_ptr> coreSinks = {
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
            std::make_shared<SessionFileSink>(fileName, 20),
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

    void Log::AddSink(spdlog::sink_ptr sink)
    {
        s_CoreLogger->sinks().push_back(sink);
        s_ClientLogger->sinks().push_back(sink);
    }
}

