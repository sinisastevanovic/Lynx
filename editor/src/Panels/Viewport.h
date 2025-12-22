#pragma once

namespace Lynx
{
    class EditorLayer;
    class Viewport
    {
    public:
        void SetOwner(EditorLayer* owner);
        void OnImGuiRender();

    private:
        EditorLayer* m_Owner = nullptr;
        int m_CurrentGizmoOperation = 7;
    };

}
