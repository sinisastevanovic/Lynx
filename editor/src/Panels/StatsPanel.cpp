#include "StatsPanel.h"

#include <imgui.h>

#include "Lynx/Engine.h"

namespace Lynx
{
    void StatsPanel::OnImGuiRender()
    {
        ImGui::Begin("Renderer Stats");

        auto& stats = Engine::Get().GetRenderer().GetRenderStats();

        m_AccumulatedTime += stats.FrameTime;
        m_FrameCount++;

        if (m_AccumulatedTime >= 0.25f)
        {
            m_DisplayFrameTime = (m_AccumulatedTime / m_FrameCount) * 1000.0f;
            m_DisplayFPS = (float)m_FrameCount / m_AccumulatedTime;

            m_AccumulatedTime = 0.0f;
            m_FrameCount = 0;
        }

        ImGui::Text("Draw Calls: %d", stats.DrawCalls);
        ImGui::Text("Index Count: %d", stats.IndexCount);

        ImGui::Separator();

        ImGui::Text("Frame Time: %.3f ms", m_DisplayFrameTime);
        ImGui::Text("FPS: %.1f", m_DisplayFPS);

        ImGui::End();
    }
}
