#pragma once

#ifdef LX_PLATFORM_WINDOWS
    #define LX_DEBUG_BREAK __debugbreak()
#else
    #defineLX_DEBUG_BREAK
#endif

#ifdef LX_DEBUG
    #define LX_ENABLE_ASSERTS
#endif

#define LX_ENABLE_VERIFY

#ifdef LX_ENABLE_ASSERTS
    #define LX_CORE_ASSERT_MESSAGE_INTERNAL(...)    ::Lynx::Log::PrintAssertMessage(::Lynx::Log::Type::Core, "Assertion Failed", __VA_ARGS__)
    #define LX_ASSERT_MESSAGE_INTERNAL(...)    ::Lynx::Log::PrintAssertMessage(::Lynx::Log::Type::Client, "Assertion Failed", __VA_ARGS__)

    #define LX_CORE_ASSERT(condition, ...) { if(!(condition)) { LX_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); LX_DEBUG_BREAK; } }
    #define LX_ASSERT(condition, ...) { if(!(condition)) { LX_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); LX_DEBUG_BREAK; } }
#else
    #define LX_CORE_ASSERT(condition, ...)
    #define LX_ASSERT(condition, ...)
#endif

#ifdef LX_ENABLE_VERIFY
    #define LX_CORE_VERIFY_MESSAGE_INTERNAL(...)    ::Lynx::Log::PrintAssertMessage(::Lynx::Log::Type::Core, "Verify Failed", __VA_ARGS__)
    #define LX_VERIFY_MESSAGE_INTERNAL(...)    ::Lynx::Log::PrintAssertMessage(::Lynx::Log::Type::Client, "Verify Failed", __VA_ARGS__)

    #define LX_CORE_VERIFY(condition, ...) { if(!(condition)) { LX_CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); LX_DEBUG_BREAK; } }
    #define LX_VERIFY(condition, ...) { if(!(condition)) { LX_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); LX_DEBUG_BREAK; } }
#else
    #define LX_CORE_VERIFY(condition, ...)
    #define LX_VERIFY(condition, ...)
#endif