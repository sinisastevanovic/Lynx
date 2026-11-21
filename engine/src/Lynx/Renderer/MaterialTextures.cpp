#include "MaterialTextures.h"

#include "Lynx/Asset/AssetManager.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/Texture.h"

namespace Lynx
{
    MaterialTextures::MaterialTextures(AssetManager& assets, const MaterialTextureIndices& defaults)
        : m_Assets(assets), m_Defaults(defaults)
    {
    }

    MaterialTextureIndices MaterialTextures::Get(const std::shared_ptr<Material>& material)
    {
        if (!material)
            return m_Defaults;

        Entry& entry = m_Entries[material.get()];
        const bool sameMaterial = !entry.Owner.expired() && entry.Owner.lock() == material;
        if (!sameMaterial || entry.Version != material->GetVersion())
        {
            entry.Owner = material;
            entry.Version = material->GetVersion();
            entry.Indices = Resolve(*material);
        }
        return entry.Indices;
    }

    MaterialTextureIndices MaterialTextures::Resolve(const Material& material) const
    {
        MaterialTextureIndices indices = m_Defaults;

        const auto loaded = [this](AssetHandle handle) -> std::shared_ptr<Texture>
        {
            if (!handle)
                return nullptr;

            std::shared_ptr<Texture> texture = m_Assets.GetAsset<Texture>(handle);
            return texture && texture->IsOnGpu() ? texture : nullptr;
        };

        // All textures of a material are read with the sampler of its albedo texture.
        if (const std::shared_ptr<Texture> albedo = loaded(material.AlbedoTexture))
        {
            indices.Albedo = albedo->GetTableIndex();
            indices.Sampler = albedo->GetSamplerIndex();
        }
        if (const std::shared_ptr<Texture> normal = loaded(material.NormalMap))
            indices.Normal = normal->GetTableIndex();
        if (const std::shared_ptr<Texture> metallicRoughness = loaded(material.MetallicRoughnessTexture))
            indices.MetallicRoughness = metallicRoughness->GetTableIndex();
        if (const std::shared_ptr<Texture> emissive = loaded(material.EmissiveTexture))
            indices.Emissive = emissive->GetTableIndex();

        return indices;
    }
}
