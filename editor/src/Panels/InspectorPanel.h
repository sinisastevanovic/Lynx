#pragma once
#include <Lynx.h>

#include "EditorPanel.h"

namespace Lynx
{
    class InspectorPanel : public EditorPanel
    {
    public:
        InspectorPanel() = default;

        virtual void OnImGuiRender() override;
        virtual void OnSceneContextChanged(Scene* context) override;
        virtual void OnSelectedEntityChanged(entt::entity selectedEntity) override;
        virtual void OnSelectedUIElementChanged(std::shared_ptr<UIElement> uiElement) override;
        
    private:
        Scene* m_Context = nullptr;
        entt::entity m_Selection = entt::null;
        std::shared_ptr<UIElement> m_SelectedUIElement{ nullptr };
    };

}
