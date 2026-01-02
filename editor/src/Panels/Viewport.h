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

    private:
        entt::entity m_Selection{ entt::null };
        std::function<void(entt::entity)> OnSelectionChangedCallback = nullptr;
        int m_CurrentGizmoOperation = 7;
    };

}
