#include "AssetSpecification.h"

#include "MeshSpecification.h"
#include "TextureSpecification.h"

namespace Lynx
{
    std::shared_ptr<AssetSpecification> AssetSpecification::CreateFromType(AssetType type)
    {
        switch (type)
        {
            case AssetType::Texture: return std::make_shared<TextureSpecification>();
            case AssetType::StaticMesh: return std::make_shared<StaticMeshSpecification>();
            default: return nullptr;
        }
    }
}
