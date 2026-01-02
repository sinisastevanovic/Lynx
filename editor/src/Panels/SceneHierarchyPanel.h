#pragma once
#include <memory>

#include "EditorPanel.h"

namespace Lynx
{
    class SceneHierarchyPanel : public EditorPanel
    {
    public:
        SceneHierarchyPanel(const std::function<void(entt::entity)>& selectionChangedCallback)
            : OnSelectionChangedCallback(selectionChangedCallback) {}

        virtual void OnImGuiRender() override;
        virtual void OnSceneContextChanged(Scene* context) override;
        virtual void OnSelectedEntityChanged(entt::entity selectedEntity) override;

    private:
        void DrawEntityNode(entt::entity entity, bool isSelected);

    private:
        Scene* m_Context = nullptr;
        entt::entity m_Selection{ entt::null };
        std::function<void(entt::entity)> OnSelectionChangedCallback = nullptr;
    };
}


