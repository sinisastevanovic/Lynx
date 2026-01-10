#pragma once
#include "Lynx/UUID.h"
#include "Lynx/Asset/AssetTypes.h"
#include "Lynx/Scene/Components/LuaScriptComponent.h"

namespace Lynx
{
    class Scene;
    class LX_API EditorUIHelpers
    {
    public:
        static bool DrawAssetSelection(const char* label, AssetHandle& currentHandle, std::initializer_list<AssetType> allowedTypes);
        static bool DrawUIElementSelection(const char* label, UUID& id, Scene* context);
        static void DrawLuaScriptSection(LuaScriptComponent* lsc);
    };
}

