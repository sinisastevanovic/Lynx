#pragma once

#include "Asset.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
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

    // A terrain is cut into square tiles of this many quads a side, unless it is smaller or told otherwise.
    constexpr uint32_t TerrainDefaultTileQuads = 512;

    // Around the heights and the weights of a tile lies one row of its neighbours', so that what is
    // read between two places, or to both sides of one, comes out the same on either side of an edge.
    // Has to match Terrain.slang.
    constexpr uint32_t TerrainTileBorder = 1;

    // One kind of ground of a terrain: grass, rock, sand. Its look is a material like any other, laid
    // flat on the ground and repeated every TileSize units.
    struct TerrainLayer
    {
        std::shared_ptr<Material> Material;
        float TileSize = 4.0f;
    };

    // The ground of a landscape: a square of heights. Heights are 16-bit numbers and stand for the
    // range from Lowest to Highest. The square is Size units long each way, whatever its resolution.
    //
    // Inside, a terrain is cut into square tiles, each with its own pictures of heights and weights.
    // To whoever uses a terrain it is one thing: one asset, one entity, one list of layers. The tiles
    // are there so that a large landscape can later be loaded and dropped piece by piece (stage 6).
    // For now all tiles of a terrain are loaded.
    //
    // The resolution is the number of heights a side. One less than it has to be the quads of a tile
    // times a whole number of tiles, and the quads of a tile those of a patch times a power of two.
    // So with tiles of 512 quads: 513, 1025, 1537, 2049 and so on. Heights beyond the last tile that
    // fits are left out.
    //
    // What the ground looks like is up to four layers, see TerrainLayer, and weights that say how much
    // of each layer shows where: four bytes per place, one per layer, at a resolution of their own.
    // Without layers a terrain is drawn in two plain colours, by how steep it is.
    //
    // A terrain is made in code from its heights, or loaded from an .lxterrain file, see
    // TerrainSerializer for what that holds. Save writes one made in code to such a file.
    class LX_API Terrain : public Asset
    {
    public:
        // Made in code, on the main thread: the heights go to the GPU at once. heights holds resolution
        // by resolution numbers, row by row from the corner with the smallest x and z, x first.
        Terrain(uint32_t resolution, float size, float lowest, float highest, std::span<const uint16_t> heights,
            uint32_t tileQuads = TerrainDefaultTileQuads);

        // Loaded from a file by the asset manager, which reads it on any thread (LoadSourceData) and
        // brings it to the GPU on the main thread (CreateRenderResources).
        explicit Terrain(const std::string& filePath);
        ~Terrain() override;

        // Loads a terrain from a file here and now, without the asset manager. On the main thread.
        // Null if the file cannot be read.
        static std::shared_ptr<Terrain> LoadFromFile(const std::filesystem::path& filePath);

        // Writes the terrain to an .lxterrain file and its tiles into a folder next to it. The layers
        // are saved by the handles of their materials, so those have to be assets the next load can find.
        bool Save(const std::filesystem::path& filePath) const;

        static AssetType GetStaticType() { return AssetType::Terrain; }
        AssetType GetType() const override { return GetStaticType(); }

        float GetSize() const { return m_Size; }
        float GetLowest() const { return m_Lowest; }
        float GetHighest() const { return m_Highest; }

        // The heights a side of the whole terrain.
        uint32_t GetResolution() const { return m_TilesPerSide * m_TileQuads + 1; }

        // The tiles: how many lie side by side, how many quads one is long, and its length in units.
        uint32_t GetTilesPerSide() const { return m_TilesPerSide; }
        uint32_t GetTileQuads() const { return m_TileQuads; }
        float GetTileSize() const { return m_TilesPerSide > 0 ? m_Size / static_cast<float>(m_TilesPerSide) : 0.0f; }

        // The sizes the patches of a tile come in: level 0 is the smallest, and each further one twice
        // as long a side, up to the one that is the tile.
        uint32_t GetLevelCount() const;

        // How many patches of a level lie side by side in a tile.
        uint32_t GetPatchesPerSide(uint32_t level) const { return (m_TileQuads / TerrainQuadsPerPatch) >> level; }

        // The lowest and the highest height within a patch of a tile, for leaving out patches that are not in view.
        struct HeightRange
        {
            float Lowest = 0.0f;
            float Highest = 0.0f;
        };
        HeightRange GetHeightsOfPatch(uint32_t tileX, uint32_t tileZ, uint32_t level, uint32_t x, uint32_t z) const;

        // The height at a place of the whole terrain, counted in steps of the heights from the first corner.
        float GetHeightAt(uint32_t x, uint32_t z) const;

        // Gives the terrain its ground. weights holds four bytes for each of resolution by resolution
        // places of the whole terrain, row by row like the heights. The resolution has to be a whole
        // number of times the number of tiles a side. Call on the main thread: the weights go to the
        // GPU at once.
        void SetLayers(std::span<const TerrainLayer> layers, uint32_t resolution, std::span<const uint8_t> weights);

        // Changes the layers and keeps the weights. A terrain that has no weights yet gets ones in
        // which the first layer shows everywhere. No layers at all gives the plain colours back.
        void SetLayers(std::span<const TerrainLayer> layers);

        // The layers that have a material, in order.
        std::span<const TerrainLayer> GetLayers() const { return std::span<const TerrainLayer>(m_Layers.data(), m_LayerCount); }

        // How many texels a side the pictures of a tile have, their borders included.
        uint32_t GetHeightTexels() const { return m_TileQuads + 1 + 2 * TerrainTileBorder; }
        uint32_t GetWeightTexels() const { return m_WeightTileResolution + 2 * TerrainTileBorder; }

        // Where the texture table has the heights and the weights of a tile, and the sampler table the
        // sampler they are read with. Only meaningful once the terrain is on the GPU.
        bool IsOnGpu() const { return m_IsOnGpu; }
        uint32_t GetHeightTableIndex(uint32_t tileX, uint32_t tileZ) const;
        uint32_t GetWeightTableIndex(uint32_t tileX, uint32_t tileZ) const;
        uint32_t GetHeightSamplerIndex() const { return m_HeightSampler; }

        bool LoadSourceData() override;
        bool CreateRenderResources() override;

    protected:
        // Reads the terrain from its file again, on the main thread.
        bool Reload() override;

    private:
        // The lowest and highest stored height of a patch.
        struct StoredRange
        {
            uint16_t Lowest = 65535;
            uint16_t Highest = 0;
        };

        // One tile: its heights and weights with their borders, as they go to the GPU and into its files.
        struct Tile
        {
            std::vector<uint16_t> Heights;
            std::vector<uint8_t> Weights;

            // Per level, the range of every patch, row by row.
            std::vector<std::vector<StoredRange>> PatchHeights;

            std::unique_ptr<GpuTexture> HeightMap;
            std::unique_ptr<TextureSlot> HeightSlot;
            std::unique_ptr<GpuTexture> WeightMap;
            std::unique_ptr<TextureSlot> WeightSlot;
        };

        const Tile& TileAt(uint32_t tileX, uint32_t tileZ) const { return m_Tiles[static_cast<size_t>(tileZ) * m_TilesPerSide + tileX]; }
        float ToHeight(uint16_t stored) const { return m_Lowest + (m_Highest - m_Lowest) * (static_cast<float>(stored) / 65535.0f); }

        // Sets how the terrain is cut for a resolution, and false if no tile fits into it.
        bool SetTiling(uint32_t resolution, uint32_t tileQuads);
        void MeasurePatches(Tile& tile) const;
        bool UploadWeights();

    private:
        float m_Size = 0.0f;
        float m_Lowest = 0.0f;
        float m_Highest = 0.0f;

        uint32_t m_TilesPerSide = 0;
        uint32_t m_TileQuads = 0;

        // The weights of a tile without their border, a side.
        uint32_t m_WeightTileResolution = 0;
        std::vector<Tile> m_Tiles;

        bool m_IsOnGpu = false;
        uint32_t m_HeightSampler = 0;

        std::array<TerrainLayer, TerrainLayerCount> m_Layers;
        size_t m_LayerCount = 0;

        // The materials of a terrain that was read from a file are known by their handles until it
        // comes to the GPU, where they are looked up.
        struct PendingLayer
        {
            AssetHandle Material = AssetHandle::Null();
            float TileSize = 4.0f;
        };
        std::vector<PendingLayer> m_PendingLayers;
    };
}
