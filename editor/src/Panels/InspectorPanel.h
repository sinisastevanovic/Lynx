#pragma once
#include <Lynx.h>

namespace Lynx
{
    class EditorLayer;
    class InspectorPanel
    {
    public:
        InspectorPanel(EditorLayer* owner) : m_Owner(owner) {}

        void OnImGuiRender(std::shared_ptr<Scene> context, ComponentRegistry& registry);

    private:
        EditorLayer* m_Owner;
    };

}
