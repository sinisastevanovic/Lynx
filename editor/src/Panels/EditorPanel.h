#pragma once

namespace Lynx
{
    class EditorPanel
    {
    public:
        virtual ~EditorPanel() = default;
        virtual void OnImGuiRender() = 0;
    };
}
