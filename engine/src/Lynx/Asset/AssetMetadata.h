#pragma once
#include "AssetSpecification.h"
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

        std::shared_ptr<AssetSpecification> Specification;

        operator bool() const { return Handle != AssetHandle::Null(); }
    };
}
