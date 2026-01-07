#pragma once
#include "EditorPanel.h"

namespace Lynx
{
    class Viewport : public EditorPanel
    {
    public:
        Viewport(const std::function<void(entt::entity)>& selectionChangedCallback)
            : OnSelectionChangedCallback(selectionChangedCallback) {}
        
        virtual void OnImGuiRender() override;
        virtual void OnSelectedEntityChanged(entt::entity selectedEntity) override;
        virtual void OnSelectedUIElementChanged(std::shared_ptr<UIElement> uiElement) override;

    private:
        entt::entity m_Selection{ entt::null };
        std::shared_ptr<UIElement> m_SelectedUIElement = nullptr;
        std::function<void(entt::entity)> OnSelectionChangedCallback = nullptr;
        int m_CurrentGizmoOperation = 7;
    };

}
