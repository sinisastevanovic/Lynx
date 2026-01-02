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
        
    private:
        Scene* m_Context = nullptr;
        entt::entity m_Selection{ entt::null };
    };

}
