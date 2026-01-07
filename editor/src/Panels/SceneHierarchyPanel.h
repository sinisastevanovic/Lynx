#pragma once
#include <memory>

#include "EditorPanel.h"

namespace Lynx
{
    class UIElement;
}

namespace Lynx
{
    class SceneHierarchyPanel : public EditorPanel
    {
    public:
        SceneHierarchyPanel(const std::function<void(entt::entity)>& selectionChangedCallback, const std::function<void(std::shared_ptr<UIElement>)>& uiSelectionChangedCallback)
            : OnSelectionChangedCallback(selectionChangedCallback), OnSelectedUIElementChangedCallback(uiSelectionChangedCallback) {}

        virtual void OnImGuiRender() override;
        virtual void OnSceneContextChanged(Scene* context) override;
        virtual void OnSelectedEntityChanged(entt::entity selectedEntity) override;

    private:
        void DrawEntityNode(entt::entity entity, bool isSelected);
        void DrawUINode(std::shared_ptr<UIElement> element);

    private:
        Scene* m_Context = nullptr;
        entt::entity m_Selection{ entt::null };
        std::shared_ptr<UIElement> m_SelectedUIElement = nullptr;
        entt::entity m_DeferredDeletion{ entt::null };
        std::function<void(entt::entity)> OnSelectionChangedCallback = nullptr;
        std::function<void(std::shared_ptr<UIElement>)> OnSelectedUIElementChangedCallback = nullptr;
    };
}


