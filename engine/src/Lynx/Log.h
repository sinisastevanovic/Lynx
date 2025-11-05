#pragma once

#include "Core.h"
#include <memory>

#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace Lynx
{
    class LX_API Log
    {
    public:
        static void Init();
        static void Shutdown();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    
    };
}

#define LX_CORE_TRACE(...)    ::Lynx::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LX_CORE_INFO(...)     ::Lynx::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LX_CORE_WARN(...)     ::Lynx::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LX_CORE_ERROR(...)    ::Lynx::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LX_CORE_CRITICAL(...) ::Lynx::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define LX_TRACE(...)           ::Lynx::Log::GetClientLogger()->trace(__VA_ARGS__)
#define LX_INFO(...)            ::Lynx::Log::GetClientLogger()->info(__VA_ARGS__)
#define LX_WARN(...)            ::Lynx::Log::GetClientLogger()->warn(__VA_ARGS__)
#define LX_ERROR(...)           ::Lynx::Log::GetClientLogger()->error(__VA_ARGS__)
#define LX_CRITICAL(...)        ::Lynx::Log::GetClientLogger()->critical(__VA_ARGS__)

#ifndef NDEBUG
// Platform-specific debugger break
    #if defined(_WIN32)
        #define DEBUGBREAK() __debugbreak()
    #elif defined(__linux__) || defined(__APPLE__)
        #include <signal.h>
        #define DEBUGBREAK() raise(SIGTRAP)
    #else
        #error "Platform not supported for DEBUGBREAK"
    #endif

    #define LX_ASSERT(condition, ...) \
        do { \
            if (!(condition)) { \
                LX_CORE_CRITICAL("Assertion Failed at {}:{}", __FILE__, __LINE__); \
                LX_CORE_CRITICAL("Condition: {}", #condition); \
                LX_CORE_CRITICAL(__VA_ARGS__); \
                DEBUGBREAK(); \
            } \
    } while(0)
#else
    #define DEBUGBREAK()
    #define ENGINE_ASSERT(condition, ...)
#endif