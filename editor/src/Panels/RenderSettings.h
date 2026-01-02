#pragma once
#include "EditorPanel.h"

namespace Lynx
{
    class RenderSettings : public EditorPanel
    {
    public:
        virtual void OnImGuiRender() override;
    };

}
