#pragma once

#include <cstdint>
#include <memory>

#ifdef _WIN32
    // Check if we are building the ENGINE DLL itself (exporting)
    #ifdef LX_BUILD_DLL
        #define LX_API __declspec(dllexport)
    // Otherwise, we are a client using the ENGINE DLL (importing)
    #else
        #define LX_API __declspec(dllimport)
    #endif
#else
    // For other platforms like Linux/macOS, GCC visibility is handled differently
    #define LX_API
#endif

#define LX_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }