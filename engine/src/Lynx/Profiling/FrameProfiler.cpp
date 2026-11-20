#include "FrameProfiler.h"

#include <algorithm>

namespace Lynx
{
    void FrameProfiler::NewFrame()
    {
        const Clock::time_point now = Clock::now();

        if (m_HasFrameStart)
        {
            m_Current.Frame = m_Frame;
            m_Current.FrameMs = std::chrono::duration<double, std::milli>(now - m_FrameStart).count();

            if (m_Recording)
            {
                m_Recorded.FrameMs.push_back(m_Current.FrameMs);
                for (const Value& value : m_Current.CpuMs)
                    FindOrAdd(m_Recorded.CpuMs, value.Name).Samples.push_back(value.Amount);
                for (const Value& value : m_Current.Counters)
                    FindOrAdd(m_Recorded.Counters, value.Name).Samples.push_back(value.Amount);

                if (!m_HasRecordedFrames)
                    m_FirstRecordedFrame = m_Frame;
                m_LastRecordedFrame = m_Frame;
                m_HasRecordedFrames = true;
            }

            // GPU times are not per frame here: keep the newest known ones on display.
            m_Current.GpuMs = m_Latest.GpuMs;
            m_Latest = m_Current;
        }

        if (m_RecordFromNextFrame)
        {
            m_Recording = true;
            m_RecordFromNextFrame = false;
        }

        ++m_Frame;
        m_FrameStart = now;
        m_HasFrameStart = true;

        for (Value& value : m_Current.CpuMs)
            value.Amount = 0.0;
    }

    void FrameProfiler::AddCpuTime(std::string_view name, double milliseconds)
    {
        FindOrAdd(m_Current.CpuMs, name).Amount += milliseconds;
    }

    void FrameProfiler::SetCounter(std::string_view name, double amount)
    {
        FindOrAdd(m_Current.Counters, name).Amount = amount;
    }

    void FrameProfiler::AddGpuTime(uint64_t frame, std::string_view name, double milliseconds)
    {
        FindOrAdd(m_Latest.GpuMs, name).Amount = milliseconds;

        if (m_HasRecordedFrames && frame >= m_FirstRecordedFrame && frame <= m_LastRecordedFrame)
            FindOrAdd(m_Recorded.GpuMs, name).Samples.push_back(milliseconds);
    }

    void FrameProfiler::StartRecording()
    {
        m_Recorded = Recording();
        m_HasRecordedFrames = false;
        m_Recording = false;
        m_RecordFromNextFrame = true;
    }

    void FrameProfiler::StopRecording()
    {
        m_Recording = false;
        m_RecordFromNextFrame = false;
    }

    FrameProfiler::Value& FrameProfiler::FindOrAdd(std::vector<Value>& values, std::string_view name)
    {
        const auto found = std::ranges::find_if(values, [name](const Value& value) { return value.Name == name; });
        if (found != values.end())
            return *found;

        return values.emplace_back(Value{ std::string(name), 0.0 });
    }

    FrameProfiler::Series& FrameProfiler::FindOrAdd(std::vector<Series>& series, std::string_view name)
    {
        const auto found = std::ranges::find_if(series, [name](const Series& entry) { return entry.Name == name; });
        if (found != series.end())
            return *found;

        return series.emplace_back(Series{ std::string(name), {} });
    }
}
