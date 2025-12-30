#include "Material.h"

namespace Lynx
{
    Material::Material(const std::string& filepath)
        : Asset(filepath)
    {
        Material::Reload();
    }

    Material::Material()
    {
    }

    bool Material::DependsOn(AssetHandle handle) const
    {
        return AlbedoTexture == handle ||
                NormalMap == handle ||
                MetallicRoughnessTexture == handle ||
                EmissiveTexture == handle;
    }

    bool Material::Reload()
    {
        return true;
    }
}
