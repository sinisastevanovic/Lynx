#pragma once

#include "Lynx/Core.h"
#include "Lynx/Profiling/FrameProfiler.h"

#include <string>
#include <vector>

namespace Lynx
{
    // Draws the profiler's latest values as a small window in a corner of the screen.
    // The numbers are averaged over a short time, because single frames change too fast to read.
    class LX_API ProfilerOverlay
    {
    public:
        // Call between ImGui::NewFrame and ImGui::Render.
        void Draw(const FrameProfiler::Snapshot& snapshot);

    private:
        struct Average
        {
            std::string Name;
            double Amount = 0.0;
        };

        double Smooth(std::vector<Average>& averages, const std::string& name, double amount);

    private:
        double m_FrameMs = 0.0;
        std::vector<Average> m_CpuMs;
        std::vector<Average> m_GpuMs;
    };
}
