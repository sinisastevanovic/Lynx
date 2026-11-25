#pragma once

#include "Lynx/Asset/Asset.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Lynx
{
    // A terrain as its files hold it, before it is an asset.
    struct TerrainSource
    {
        uint32_t Resolution = 0;
        float Size = 0.0f;
        float Lowest = 0.0f;
        float Highest = 0.0f;
        std::vector<uint16_t> Heights;

        struct Layer
        {
            AssetHandle Material = AssetHandle::Null();
            float TileSize = 4.0f;
        };
        std::vector<Layer> Layers;

        uint32_t WeightResolution = 0;
        std::vector<uint8_t> Weights;
    };

    // Reads and writes a terrain's files.
    //
    // A terrain is a small text file, .lxterrain, that names two pictures next to it:
    //   Heights: square, one 16-bit number per place. Either a raw file of nothing but those numbers,
    //     the low byte first (.r16 or .raw, what terrain tools export), or a 16-bit grey PNG.
    //   Weights: square, four bytes per place, how much of each layer shows there. A PNG. Only with layers.
    // The text file holds the terrain's size, the heights that the smallest and the largest number
    // stand for, and the layers: a material and how many units one repeat of its textures covers.
    class TerrainSerializer
    {
    public:
        // Writes the text file and, next to it, <name>.heights.r16 and, with layers, <name>.weights.png.
        static bool Serialize(const std::filesystem::path& filePath, const TerrainSource& terrain);

        // Reads the text file and the pictures it names. May run on any thread.
        static bool Deserialize(const std::filesystem::path& filePath, TerrainSource& outTerrain);
    };
}
