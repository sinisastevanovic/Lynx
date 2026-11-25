#pragma once

#include "Asset.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Lynx
{
    class GpuTexture;
    class Material;
    class TextureSlot;

    // A terrain is drawn as square patches of this many quads a side, large ones far away and small
    // ones close by, see TerrainRenderer. The smallest patch has one quad per step of the heights.
    // Has to match Terrain.slang.
    constexpr uint32_t TerrainQuadsPerPatch = 32;

    // How many kinds of ground a terrain can mix. Has to match Terrain.slang.
    constexpr size_t TerrainLayerCount = 4;

    // One kind of ground of a terrain: grass, rock, sand. Its look is a material like any other, laid
    // flat on the ground and repeated every TileSize units.
    struct TerrainLayer
    {
        std::shared_ptr<Material> Material;
        float TileSize = 4.0f;
    };

    // The ground of a landscape: a square of heights. Heights are 16-bit numbers, row by row from the
    // corner with the smallest x and z, x first, and stand for the range from Lowest to Highest.
    // The square is Size units long each way, whatever its resolution.
    //
    // The resolution is the number of heights a side. One less than it has to be the quads of a patch
    // times a power of two, so 33, 65, 129 and so on up to 2049 or 4097: a patch then always covers
    // whole steps of the heights. Other resolutions are cut down to the next one that fits.
    //
    // What the ground looks like is up to four layers, see TerrainLayer, and a picture of weights that
    // says how much of each layer shows where: four bytes per place, one per layer, stretched over the
    // whole terrain at a resolution of its own. Without layers a terrain is drawn in two plain colours,
    // by how steep it is.
    //
    // For now a terrain is made in code. Loading one from a file comes with the rest of step 3.7.
    class LX_API Terrain : public Asset
    {
    public:
        Terrain(uint32_t resolution, float size, float lowest, float highest, std::vector<uint16_t> heights);
        ~Terrain() override;

        static AssetType GetStaticType() { return AssetType::Terrain; }
        AssetType GetType() const override { return GetStaticType(); }

        uint32_t GetResolution() const { return m_Resolution; }
        float GetSize() const { return m_Size; }
        float GetLowest() const { return m_Lowest; }
        float GetHighest() const { return m_Highest; }

        // The sizes patches come in: level 0 is the smallest, and each further one twice as long a side.
        uint32_t GetLevelCount() const { return static_cast<uint32_t>(m_PatchHeights.size()); }

        // How many patches of a level lie side by side.
        uint32_t GetPatchesPerSide(uint32_t level) const { return ((m_Resolution - 1) / TerrainQuadsPerPatch) >> level; }

        // The lowest and the highest height within a patch, for leaving out patches that are not in view.
        struct HeightRange
        {
            float Lowest = 0.0f;
            float Highest = 0.0f;
        };
        HeightRange GetHeightsOfPatch(uint32_t level, uint32_t x, uint32_t z) const;

        // The height at a place, counted in steps of the heights from the first corner.
        float GetHeightAt(uint32_t x, uint32_t z) const;

        // Gives the terrain its ground. weights holds four bytes for each of resolution by resolution
        // places, row by row like the heights. Call on the main thread: the weights go to the GPU at once.
        void SetLayers(std::span<const TerrainLayer> layers, uint32_t resolution, std::vector<uint8_t> weights);

        // The layers that have a material, in order, and where the texture table has their weights.
        std::span<const TerrainLayer> GetLayers() const { return std::span<const TerrainLayer>(m_Layers.data(), m_LayerCount); }
        uint32_t GetWeightTableIndex() const;

        // Where the texture table has the heights, and the sampler table the sampler they are read with.
        // Only meaningful once the terrain is on the GPU.
        bool IsOnGpu() const { return m_HeightMap != nullptr; }
        uint32_t GetHeightTableIndex() const;
        uint32_t GetHeightSamplerIndex() const { return m_HeightSampler; }

        bool CreateRenderResources() override;

    private:
        float ToHeight(uint16_t stored) const { return m_Lowest + (m_Highest - m_Lowest) * (static_cast<float>(stored) / 65535.0f); }

    private:
        uint32_t m_Resolution = 0;
        float m_Size = 0.0f;
        float m_Lowest = 0.0f;
        float m_Highest = 0.0f;
        std::vector<uint16_t> m_Heights;

        // Per level, the lowest and highest stored height of every patch, row by row.
        struct StoredRange
        {
            uint16_t Lowest = 65535;
            uint16_t Highest = 0;
        };
        std::vector<std::vector<StoredRange>> m_PatchHeights;

        std::unique_ptr<GpuTexture> m_HeightMap;
        std::unique_ptr<TextureSlot> m_HeightSlot;
        uint32_t m_HeightSampler = 0;

        std::array<TerrainLayer, TerrainLayerCount> m_Layers;
        size_t m_LayerCount = 0;
        std::unique_ptr<GpuTexture> m_WeightMap;
        std::unique_ptr<TextureSlot> m_WeightSlot;
    };
}
