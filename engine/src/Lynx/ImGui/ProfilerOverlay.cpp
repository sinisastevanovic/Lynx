#include "ProfilerOverlay.h"

#include <imgui.h>

#include <algorithm>

namespace Lynx
{
    namespace
    {
        // The share of a new frame in the displayed average. Small enough to read, large enough to follow a change within a second.
        constexpr double SmoothingFactor = 0.05;

        void DrawRow(const char* name, const char* format, double amount)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name);
            ImGui::TableNextColumn();
            ImGui::Text(format, amount);
        }
    }

    void ProfilerOverlay::Draw(const FrameProfiler::Snapshot& snapshot)
    {
        m_FrameMs = m_FrameMs == 0.0 ? snapshot.FrameMs : m_FrameMs + (snapshot.FrameMs - m_FrameMs) * SmoothingFactor;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({ viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + 10.0f }, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.75f);

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("##ProfilerOverlay", nullptr, flags))
        {
            ImGui::Text("Frame %.2f ms  (%.0f fps)", m_FrameMs, m_FrameMs > 0.0 ? 1000.0 / m_FrameMs : 0.0);

            if (ImGui::BeginTable("##ProfilerValues", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("GPU ms");
                for (const FrameProfiler::Value& value : snapshot.GpuMs)
                    DrawRow(value.Name.c_str(), "%.3f", Smooth(m_GpuMs, value.Name, value.Amount));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("CPU ms");
                for (const FrameProfiler::Value& value : snapshot.CpuMs)
                    DrawRow(value.Name.c_str(), "%.3f", Smooth(m_CpuMs, value.Name, value.Amount));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("Counters");
                for (const FrameProfiler::Value& value : snapshot.Counters)
                    DrawRow(value.Name.c_str(), "%.0f", value.Amount);

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    double ProfilerOverlay::Smooth(std::vector<Average>& averages, const std::string& name, double amount)
    {
        const auto found = std::ranges::find_if(averages, [&name](const Average& average) { return average.Name == name; });
        if (found == averages.end())
            return averages.emplace_back(Average{ name, amount }).Amount;

        found->Amount += (amount - found->Amount) * SmoothingFactor;
        return found->Amount;
    }
}
