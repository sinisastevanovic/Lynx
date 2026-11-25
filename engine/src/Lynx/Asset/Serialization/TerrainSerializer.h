#pragma once

#include "Lynx/Asset/Asset.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Lynx
{
    // A terrain as its files hold it, before it is an asset: cut into tiles, see Terrain.
    struct TerrainSource
    {
        float Size = 0.0f;
        float Lowest = 0.0f;
        float Highest = 0.0f;

        uint32_t TilesPerSide = 0;
        uint32_t TileQuads = 0;

        // The weights of a tile a side, without their border. Zero for a terrain without layers.
        uint32_t WeightTileResolution = 0;

        struct Layer
        {
            AssetHandle Material = AssetHandle::Null();
            float TileSize = 4.0f;
        };
        std::vector<Layer> Layers;

        // The heights and weights of each tile with their borders, tile by tile, row by row.
        struct Tile
        {
            std::vector<uint16_t> Heights;
            std::vector<uint8_t> Weights;
        };
        std::vector<Tile> Tiles;
    };

    // Reads and writes a terrain's files.
    //
    // A terrain is a small text file, .lxterrain, and a folder next to it, <name>.tiles, that holds
    // two files per tile: heights_<x>_<z>.r16, 16-bit numbers with the low byte first, and, with layers,
    // weights_<x>_<z>.rgba, four bytes per place. Both are nothing but the numbers, borders included, so
    // that one tile can later be read on its own. Their endings are none the engine takes for an asset.
    // The text file holds the terrain's size, the heights that the smallest and the largest number
    // stand for, how it is cut into tiles, and the layers: a material and how many units one repeat of
    // its textures covers.
    //
    // A terrain comes into being from pictures made elsewhere: ReadHeights and ReadWeights read those.
    class LX_API TerrainSerializer
    {
    public:
        static bool Serialize(const std::filesystem::path& filePath, const TerrainSource& terrain);

        // Reads the text file and the tiles. May run on any thread.
        static bool Deserialize(const std::filesystem::path& filePath, TerrainSource& outTerrain);

        // Reads a square of heights as terrain tools export it: a raw file of 16-bit numbers, the low
        // byte first (.r16 or .raw), or a 16-bit grey PNG.
        static bool ReadHeights(const std::filesystem::path& path, uint32_t& outResolution, std::vector<uint16_t>& outHeights);

        // Reads a square picture of weights, four bytes per place, from any picture file the engine reads.
        static bool ReadWeights(const std::filesystem::path& path, uint32_t& outResolution, std::vector<uint8_t>& outWeights);
    };
}
