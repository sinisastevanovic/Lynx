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
        
    public:

        static void Init(const std::string& appName);
        static void Shutdown();

        static void AddSink(spdlog::sink_ptr sink);

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

        template<typename... Args>
        static void PrintAssertMessage(Log::Type type, std::string_view prefix, Args&&... args);

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

namespace Lynx
{
    template <typename... Args>
    void Log::PrintAssertMessage(Log::Type type, std::string_view prefix, Args&&... args)
    {
        auto logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
        logger->error("{0}: {1}", prefix, fmt::format(std::forward<Args>(args)...));
    }

    template<>
    inline void Log::PrintAssertMessage(Log::Type type, std::string_view prefix)
    {
        auto logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
        logger->error("{0}", prefix);
    }
}