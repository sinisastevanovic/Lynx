#pragma once

#include "Lynx/Core.h"
#include <filesystem>

namespace Lynx
{
    enum class AssetType : uint16_t
    {
        None = 0,
        Texture,
        StaticMesh,
        SkeletalMesh,
        Material,
        Shader,
        Scene,
        Script,
        Sprite,
        Font
    };
    
    namespace AssetUtils
    {
        LX_API const char* GetFilterForSupportedAssets();
        LX_API const char* GetFilterForAssetType(AssetType type);
        LX_API AssetType GetAssetTypeFromExtension(const std::filesystem::path& path);
        LX_API bool IsAssetExtensionSupported(const std::filesystem::path& path);
        LX_API const char* GetDragDropPayload(AssetType type);
    }
}