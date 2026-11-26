#include "TerrainSerializer.h"

#include "Lynx/Asset/Terrain.h"

#include <nlohmann/json.hpp>
#include <stb_image.h>

#include <cctype>
#include <cmath>
#include <format>
#include <fstream>

namespace Lynx
{
    namespace
    {
        constexpr size_t BytesPerWeight = TerrainLayerCount;

        bool EndsWith(const std::filesystem::path& path, const char* extension)
        {
            std::string actual = path.extension().string();
            for (char& letter : actual)
                letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            return actual == extension;
        }

        // The folder a terrain's tiles are kept in, next to its text file.
        std::filesystem::path TileFolder(const std::filesystem::path& filePath)
        {
            return filePath.parent_path() / (filePath.stem().string() + ".tiles");
        }

        std::filesystem::path TileFile(const std::filesystem::path& folder, const char* what, uint32_t x, uint32_t z, const char* ending)
        {
            return folder / std::format("{}_{}_{}.{}", what, x, z, ending);
        }

        template<typename Number>
        bool WriteNumbers(const std::filesystem::path& path, const std::vector<Number>& numbers)
        {
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(numbers.data()), static_cast<std::streamsize>(numbers.size() * sizeof(Number)));
            return static_cast<bool>(file);
        }

        // Reads a file that has to hold exactly this many numbers.
        template<typename Number>
        bool ReadNumbers(const std::filesystem::path& path, size_t count, std::vector<Number>& numbers)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file || static_cast<uint64_t>(file.tellg()) != count * sizeof(Number))
                return false;

            numbers.resize(count);
            file.seekg(0);
            file.read(reinterpret_cast<char*>(numbers.data()), static_cast<std::streamsize>(count * sizeof(Number)));
            return static_cast<bool>(file);
        }
    }

    bool TerrainSerializer::ReadHeights(const std::filesystem::path& path, uint32_t& outResolution, std::vector<uint16_t>& outHeights)
    {
        if (EndsWith(path, ".png"))
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_us* pixels = stbi_load_16(path.string().c_str(), &width, &height, &channels, 1);
            if (!pixels)
            {
                LX_CORE_ERROR("Could not read heights from {0}", path.string());
                return false;
            }

            const bool isSquare = width == height && width > 0;
            if (isSquare)
            {
                outResolution = static_cast<uint32_t>(width);
                outHeights.assign(pixels, pixels + static_cast<size_t>(width) * height);
            }
            else
            {
                LX_CORE_ERROR("{0} is {1} by {2}. A terrain's heights have to be square.", path.string(), width, height);
            }
            stbi_image_free(pixels);
            return isSquare;
        }

        // A raw file is nothing but the numbers, so its size has to be that of a square.
        std::error_code error;
        const uint64_t byteCount = std::filesystem::file_size(path, error);
        const uint64_t count = byteCount / sizeof(uint16_t);
        const uint32_t resolution = static_cast<uint32_t>(std::llround(std::sqrt(static_cast<double>(count))));
        if (error || byteCount % sizeof(uint16_t) != 0 || static_cast<uint64_t>(resolution) * resolution != count || !ReadNumbers(path, count, outHeights))
        {
            LX_CORE_ERROR("{0} does not hold a square of 16-bit heights.", path.string());
            return false;
        }
        outResolution = resolution;
        return true;
    }

    bool TerrainSerializer::ReadWeights(const std::filesystem::path& path, uint32_t& outResolution, std::vector<uint8_t>& outWeights)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, static_cast<int>(BytesPerWeight));
        if (!pixels)
        {
            LX_CORE_ERROR("Could not read weights from {0}", path.string());
            return false;
        }

        const bool isSquare = width == height && width > 0;
        if (isSquare)
        {
            outResolution = static_cast<uint32_t>(width);
            outWeights.assign(pixels, pixels + static_cast<size_t>(width) * height * BytesPerWeight);
        }
        else
        {
            LX_CORE_ERROR("{0} is {1} by {2}. A terrain's weights have to be square.", path.string(), width, height);
        }
        stbi_image_free(pixels);
        return isSquare;
    }

    bool TerrainSerializer::Serialize(const std::filesystem::path& filePath, const TerrainSource& terrain)
    {
        const std::filesystem::path folder = TileFolder(filePath);
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        const bool hasLayers = !terrain.Layers.empty() && terrain.WeightTileResolution > 0;

        for (uint32_t z = 0; z < terrain.TilesPerSide; ++z)
        {
            for (uint32_t x = 0; x < terrain.TilesPerSide; ++x)
            {
                const TerrainSource::Tile& tile = terrain.Tiles[static_cast<size_t>(z) * terrain.TilesPerSide + x];
                if (!WriteNumbers(TileFile(folder, "heights", x, z, "r16"), tile.Heights) || (hasLayers && !WriteNumbers(TileFile(folder, "weights", x, z, "rgba"), tile.Weights)))
                {
                    LX_CORE_ERROR("Could not write the tiles of terrain {0}", filePath.string());
                    return false;
                }
            }
        }

        nlohmann::json json;
        json["Size"] = terrain.Size;
        json["Lowest"] = terrain.Lowest;
        json["Highest"] = terrain.Highest;
        json["TilesPerSide"] = terrain.TilesPerSide;
        json["TileQuads"] = terrain.TileQuads;
        if (hasLayers)
        {
            json["WeightTileResolution"] = terrain.WeightTileResolution;
            nlohmann::json& layers = json["Layers"] = nlohmann::json::array();
            for (const TerrainSource::Layer& layer : terrain.Layers)
                layers.push_back({ { "Material", static_cast<uint64_t>(layer.Material) }, { "TileSize", layer.TileSize } });
        }
        if (!terrain.ScatterRules.empty())
        {
            nlohmann::json& rules = json["Scatter"] = nlohmann::json::array();
            for (const TerrainSource::Scatter& scatter : terrain.ScatterRules)
            {
                const ScatterRule& rule = scatter.Rule;
                rules.push_back({
                    { "Mesh", static_cast<uint64_t>(scatter.Mesh) },
                    { "Spacing", rule.Spacing },
                    { "Density", rule.Density },
                    { "SmallestSize", rule.SmallestSize },
                    { "LargestSize", rule.LargestSize },
                    { "SteepestSlope", rule.SteepestSlope },
                    { "LowestHeight", rule.LowestHeight },
                    { "HighestHeight", rule.HighestHeight },
                    { "LayerShares", rule.LayerShares },
                });
            }
        }

        std::ofstream file(filePath);
        file << json.dump(4);
        if (!file)
        {
            LX_CORE_ERROR("Could not save terrain to: {0}", filePath.string());
            return false;
        }
        return true;
    }

    bool TerrainSerializer::Deserialize(const std::filesystem::path& filePath, TerrainSource& outTerrain)
    {
        std::ifstream file(filePath);
        if (!file)
        {
            LX_CORE_ERROR("Could not open terrain file: {0}", filePath.string());
            return false;
        }

        nlohmann::json json;
        try
        {
            file >> json;
        }
        catch (const nlohmann::json::exception& e)
        {
            LX_CORE_ERROR("Failed to parse terrain JSON {0}: {1}", filePath.string(), e.what());
            return false;
        }

        outTerrain = {};
        outTerrain.Size = json.value("Size", 0.0f);
        outTerrain.Lowest = json.value("Lowest", 0.0f);
        outTerrain.Highest = json.value("Highest", 0.0f);
        outTerrain.TilesPerSide = json.value("TilesPerSide", 0u);
        outTerrain.TileQuads = json.value("TileQuads", 0u);
        if (outTerrain.Size <= 0.0f || outTerrain.TilesPerSide == 0 || outTerrain.TileQuads == 0)
        {
            LX_CORE_ERROR("Terrain {0} needs a size and its tiles.", filePath.string());
            return false;
        }

        // Without layers, or without their weights, the terrain is drawn in its plain colours.
        outTerrain.WeightTileResolution = json.value("WeightTileResolution", 0u);
        if (json.contains("Layers") && json["Layers"].is_array() && outTerrain.WeightTileResolution > 0)
        {
            for (const nlohmann::json& layer : json["Layers"])
            {
                TerrainSource::Layer& read = outTerrain.Layers.emplace_back();
                read.Material = AssetHandle(layer.value("Material", uint64_t{ 0 }));
                read.TileSize = layer.value("TileSize", read.TileSize);
            }
        }

        if (json.contains("Scatter") && json["Scatter"].is_array())
        {
            for (const nlohmann::json& saved : json["Scatter"])
            {
                if (!saved.is_object())
                    continue;

                TerrainSource::Scatter& read = outTerrain.ScatterRules.emplace_back();
                ScatterRule& rule = read.Rule;
                read.Mesh = AssetHandle(saved.value("Mesh", uint64_t{ 0 }));
                rule.Spacing = saved.value("Spacing", rule.Spacing);
                rule.Density = saved.value("Density", rule.Density);
                rule.SmallestSize = saved.value("SmallestSize", rule.SmallestSize);
                rule.LargestSize = saved.value("LargestSize", rule.LargestSize);
                rule.SteepestSlope = saved.value("SteepestSlope", rule.SteepestSlope);
                rule.LowestHeight = saved.value("LowestHeight", rule.LowestHeight);
                rule.HighestHeight = saved.value("HighestHeight", rule.HighestHeight);
                if (saved.contains("LayerShares") && saved["LayerShares"].is_array() && saved["LayerShares"].size() == rule.LayerShares.size())
                    rule.LayerShares = saved["LayerShares"].get<std::array<float, 4>>();
            }
        }

        const std::filesystem::path folder = TileFolder(filePath);
        const size_t heightTexels = static_cast<size_t>(outTerrain.TileQuads) + 1 + 2 * TerrainTileBorder;
        const size_t weightTexels = static_cast<size_t>(outTerrain.WeightTileResolution) + 2 * TerrainTileBorder;
        outTerrain.Tiles.resize(static_cast<size_t>(outTerrain.TilesPerSide) * outTerrain.TilesPerSide);
        for (uint32_t z = 0; z < outTerrain.TilesPerSide; ++z)
        {
            for (uint32_t x = 0; x < outTerrain.TilesPerSide; ++x)
            {
                TerrainSource::Tile& tile = outTerrain.Tiles[static_cast<size_t>(z) * outTerrain.TilesPerSide + x];
                if (!ReadNumbers(TileFile(folder, "heights", x, z, "r16"), heightTexels * heightTexels, tile.Heights))
                {
                    LX_CORE_ERROR("Terrain {0}: the heights of tile {1}, {2} are missing or of the wrong size.", filePath.string(), x, z);
                    return false;
                }

                if (!outTerrain.Layers.empty() && !ReadNumbers(TileFile(folder, "weights", x, z, "rgba"), weightTexels * weightTexels * BytesPerWeight, tile.Weights))
                {
                    LX_CORE_ERROR("Terrain {0}: the weights of tile {1}, {2} are missing or of the wrong size. It keeps its plain colours.", filePath.string(), x, z);
                    outTerrain.Layers.clear();
                }
            }
        }
        return true;
    }
}
