#pragma once
#include <memory>

#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    class EditorLayer;
    class SceneHierarchyPanel
    {
    public:
        SceneHierarchyPanel(EditorLayer* owner);
        SceneHierarchyPanel(const std::shared_ptr<Scene>& context);

        void SetContext(const std::shared_ptr<Scene>& context);

        void OnImGuiRender();

    private:
        void DrawEntityNode(entt::entity entity, bool isSelected);

    private:
        EditorLayer* m_Owner;
        std::shared_ptr<Scene> m_Context;
    };
}


