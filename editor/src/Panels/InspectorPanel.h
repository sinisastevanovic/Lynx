#pragma once
#include <Lynx.h>

namespace Lynx
{
    class InspectorPanel
    {
    public:
        InspectorPanel() = default;

        void OnImGuiRender(std::shared_ptr<Scene> context, entt::entity selectedEntity, ComponentRegistry& registry);
    };

}
