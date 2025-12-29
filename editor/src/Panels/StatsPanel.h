#pragma once

namespace Lynx
{
    class StatsPanel
    {
    public:
        StatsPanel() = default;
        void OnImGuiRender();

    private:
        float m_AccumulatedTime = 0.0f;
        int m_FrameCount = 0;

        float m_DisplayFPS = 0.0f;
        float m_DisplayFrameTime = 0.0f;
    };

}
