#pragma once
#include <glm/vec2.hpp>

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

        glm::vec2 GetPos() const { return m_Bounds[0]; }
        glm::vec2 GetSize() const { return m_Bounds[1] - m_Bounds[0]; }
        bool IsFocused() const { return m_IsFocused; }
        bool IsHovered() const { return m_IsHovered; }
        bool IsClicked() const { return m_IsClicked; }

    private:
        entt::entity m_Selection{ entt::null };
        std::shared_ptr<UIElement> m_SelectedUIElement = nullptr;
        std::function<void(entt::entity)> OnSelectionChangedCallback = nullptr;
        int m_CurrentGizmoOperation = 7;

        glm::vec2 m_Bounds[2];
        bool m_IsFocused = false;
        bool m_IsHovered = false;
        bool m_IsClicked = false;
    };

}
