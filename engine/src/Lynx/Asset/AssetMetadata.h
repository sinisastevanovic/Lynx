#pragma once
#include "AssetTypes.h"
#include "Lynx/UUID.h"

namespace  Lynx
{
    struct AssetMetadata
    {
        AssetHandle Handle = AssetHandle::Null();
        AssetType Type = AssetType::None;
        std::filesystem::path FilePath;
        bool IsLoaded = false; // TODO: Use this or remove

        operator bool() const { return Handle != AssetHandle::Null(); }
    };
}
