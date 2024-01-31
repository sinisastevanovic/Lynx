#pragma once

#include <iostream>
#include <string>
#include <chrono>

namespace Lynx
{
    class Timer
    {
    public:
        Timer()
        {
            Reset();
        }

        void Reset()
        {
            Start_ = std::chrono::high_resolution_clock::now();
        }

        float Elapsed()
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - Start_).count() * 0.001f * 0.001f * 0.001f;
        }

        float ElapsedMs()
        {
            return Elapsed() * 1000.0f;
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> Start_;
    };

    class ScopedTimer
    {
    public:
        ScopedTimer(const std::string& name)
            : Name_(name) {}

        ~ScopedTimer()
        {
            float time = Timer_.ElapsedMs();
            std::cout << "[TIMER] " << Name_ << " - " << time << "ms\n";
        }

    private:
        std::string Name_;
        Timer Timer_;
    };
}
