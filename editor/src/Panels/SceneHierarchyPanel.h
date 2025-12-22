#pragma once
#include <memory>

#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    class SceneHierarchyPanel
    {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const std::shared_ptr<Scene>& context);

        void SetContext(const std::shared_ptr<Scene>& context);

        void OnImGuiRender();

        entt::entity GetSelectedEntity() const { return m_SelectedEntity; }
        void SetSelectedEntity(entt::entity entity) { m_SelectedEntity = entity; }

    private:
        void DrawEntityNode(entt::entity entity);

    private:
        std::shared_ptr<Scene> m_Context;
        entt::entity m_SelectedEntity = entt::null;
    };
}


