#pragma once

#include "spdlog/spdlog.h"

namespace Lynx
{
    class Log
    {
    public:
        enum class Type : uint8_t
        {
            Core = 0, Client = 1
        };
        enum class Level : uint8_t
        {
            Trace = 0, Info, Warn, Error, Fatal
        };
        
    public:

        static void Init();
        static void Shutdown();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    public:
        static const char* LevelToString(Level level)
        {
            switch (level)
            {
            case Level::Trace: return "Trace";
            case Level::Info: return "Info";
            case Level::Warn: return "Warning";
            case Level::Error: return "Error";
            case Level::Fatal: return "Fatal";
            }
            return "";
        }
        
        static Level LevelFromString(std::string_view string)
        {
            if (string == "Trace") return Level::Trace;
            if (string == "Info") return Level::Info;
            if (string == "Warning") return Level::Warn;
            if (string == "Error") return Level::Error;
            if (string == "Fatal") return Level::Fatal;

            return Level::Trace;
        }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    
    };
}

// Core Logging
#define LX_CORE_TRACE(...)  ::Lynx::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LX_CORE_INFO(...)   ::Lynx::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LX_CORE_WARN(...)   ::Lynx::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LX_CORE_ERROR(...)  ::Lynx::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LX_CORE_FATAL(...)  ::Lynx::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client Logging
#define LX_TRACE(...)   ::Lynx::Log::GetClientLogger()->trace(__VA_ARGS__)
#define LX_INFO(...)    ::Lynx::Log::GetClientLogger()->info(__VA_ARGS__)
#define LX_WARN(...)    ::Lynx::Log::GetClientLogger()->warn(__VA_ARGS__)
#define LX_ERROR(...)   ::Lynx::Log::GetClientLogger()->error(__VA_ARGS__)
#define LX_FATAL(...)   ::Lynx::Log::GetClientLogger()->critical(__VA_ARGS__)