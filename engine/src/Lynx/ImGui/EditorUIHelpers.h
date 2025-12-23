#pragma once
#include "Lynx/UUID.h"
#include "Lynx/Asset/AssetTypes.h"

namespace Lynx
{
    class EditorUIHelpers
    {
    public:
        static bool DrawAssetSelection(const char* label, AssetHandle& currentHandle, AssetType typeFilter);
    
    };
}

