#pragma once

#include "RenderPass.h"

#include <memory>
#include <unordered_map>

namespace Lynx
{
    class AssetManager;
    class Material;

    // Looks up where the texture table has the textures of a material, and remembers the answer until
    // the material changes. A material is given a new version whenever it or one of its textures is
    // loaded, edited or reloaded, so comparing versions is all it takes to notice.
    //
    // This is the seed of the material table of stage 2, where the answers move into a buffer on the GPU.
    class MaterialTextures
    {
    public:
        // The defaults stand in for a texture a material does not have, or one that is still loading.
        MaterialTextures(AssetManager& assets, const MaterialTextureIndices& defaults);

        MaterialTextureIndices Get(const std::shared_ptr<Material>& material);

    private:
        MaterialTextureIndices Resolve(const Material& material) const;

    private:
        struct Entry
        {
            // Tells a material apart from a later one that happens to live at the same address.
            std::weak_ptr<Material> Owner;
            uint32_t Version = 0;
            MaterialTextureIndices Indices;
        };

        AssetManager& m_Assets;
        MaterialTextureIndices m_Defaults;
        std::unordered_map<const Material*, Entry> m_Entries;
    };
}
