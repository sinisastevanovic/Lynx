#pragma once
#include "EditorPanel.h"

namespace Lynx
{
    class StatsPanel : public EditorPanel
    {
    public:
        StatsPanel() = default;

        virtual void OnImGuiRender() override;

    private:
        float m_AccumulatedTime = 0.0f;
        int m_FrameCount = 0;

        float m_DisplayFPS = 0.0f;
        float m_DisplayFrameTime = 0.0f;
    };

}
