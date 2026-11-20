#pragma once

#include "Lynx/Core.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Lynx
{
    // Collects what one frame cost: the frame time, named CPU and GPU times, and counters.
    // The latest values are kept for an on-screen display, and a range of frames can be recorded
    // for a benchmark. It measures nothing itself except the frame time: code reports its own times.
    class LX_API FrameProfiler
    {
    public:
        struct Value
        {
            std::string Name;
            double Amount = 0.0;
        };

        // Values of the most recent finished frame. GPU times arrive a few frames late,
        // so the GPU list holds the newest time known for each name.
        struct Snapshot
        {
            uint64_t Frame = 0;
            double FrameMs = 0.0;
            std::vector<Value> CpuMs;
            std::vector<Value> GpuMs;
            std::vector<Value> Counters;
        };

        struct Series
        {
            std::string Name;
            std::vector<double> Samples;
        };

        // Everything reported for the recorded frames, one sample per frame and name.
        struct Recording
        {
            std::vector<double> FrameMs;
            std::vector<Series> CpuMs;
            std::vector<Series> GpuMs;
            std::vector<Series> Counters;
        };

        // Ends the current frame and starts the next one. The frame time is the time between two calls.
        void NewFrame();
        uint64_t CurrentFrame() const { return m_Frame; }

        // Several reports of one name in a frame add up.
        void AddCpuTime(std::string_view name, double milliseconds);
        void SetCounter(std::string_view name, double amount);

        // GPU times are measured by queries whose results come back later,
        // so they name the frame they belong to.
        void AddGpuTime(uint64_t frame, std::string_view name, double milliseconds);

        const Snapshot& Latest() const { return m_Latest; }

        // Records every frame from the next NewFrame until StopRecording. GPU times that arrive
        // after the stop still count if their frame was recorded.
        void StartRecording();
        void StopRecording();
        bool IsRecording() const { return m_Recording; }
        const Recording& GetRecording() const { return m_Recorded; }

    private:
        static Value& FindOrAdd(std::vector<Value>& values, std::string_view name);
        static Series& FindOrAdd(std::vector<Series>& series, std::string_view name);

    private:
        using Clock = std::chrono::steady_clock;

        uint64_t m_Frame = 0;
        Clock::time_point m_FrameStart;
        bool m_HasFrameStart = false;

        Snapshot m_Current;
        Snapshot m_Latest;

        bool m_Recording = false;
        bool m_RecordFromNextFrame = false;
        uint64_t m_FirstRecordedFrame = 0;
        uint64_t m_LastRecordedFrame = 0;
        bool m_HasRecordedFrames = false;
        Recording m_Recorded;
    };

    // Reports the time between its construction and destruction as a CPU time.
    class ProfileScope
    {
    public:
        ProfileScope(FrameProfiler& profiler, std::string_view name)
            : m_Profiler(profiler), m_Name(name), m_Start(std::chrono::steady_clock::now())
        {
        }

        ~ProfileScope()
        {
            const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - m_Start;
            m_Profiler.AddCpuTime(m_Name, elapsed.count());
        }

        ProfileScope(const ProfileScope&) = delete;
        ProfileScope& operator=(const ProfileScope&) = delete;

    private:
        FrameProfiler& m_Profiler;
        std::string_view m_Name;
        std::chrono::steady_clock::time_point m_Start;
    };
}
