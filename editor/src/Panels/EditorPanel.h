#pragma once
#include <entt/entity/entity.hpp>
#include "Lynx/Asset/Asset.h"

namespace Lynx
{
    class Scene;

    class EditorPanel
    {
    public:
        virtual ~EditorPanel() = default;
        virtual void OnImGuiRender() = 0;

        virtual void OnSceneContextChanged(Scene* context) {}
        virtual void OnSelectedEntityChanged(entt::entity selectedEntity) {}
        virtual void OnSelectedAssetChanged(AssetHandle handle) {}
    };
}
