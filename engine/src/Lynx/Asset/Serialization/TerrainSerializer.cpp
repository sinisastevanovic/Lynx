#include "TerrainSerializer.h"

#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

#include <cmath>
#include <cstring>
#include <fstream>

namespace Lynx
{
    namespace
    {
        constexpr size_t BytesPerWeight = 4;

        bool EndsWith(const std::filesystem::path& path, const char* extension)
        {
            std::string actual = path.extension().string();
            for (char& letter : actual)
                letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            return actual == extension;
        }

        // A raw file is nothing but the numbers, so its size has to be that of a square.
        bool ReadRawHeights(const std::filesystem::path& path, TerrainSource& terrain)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
                return false;

            const uint64_t byteCount = static_cast<uint64_t>(file.tellg());
            const uint64_t count = byteCount / sizeof(uint16_t);
            const uint32_t resolution = static_cast<uint32_t>(std::llround(std::sqrt(static_cast<double>(count))));
            if (byteCount % sizeof(uint16_t) != 0 || static_cast<uint64_t>(resolution) * resolution != count)
            {
                LX_CORE_ERROR("{0} does not hold a square of 16-bit heights.", path.string());
                return false;
            }

            terrain.Resolution = resolution;
            terrain.Heights.resize(count);
            file.seekg(0);
            file.read(reinterpret_cast<char*>(terrain.Heights.data()), static_cast<std::streamsize>(byteCount));
            return static_cast<bool>(file);
        }

        bool ReadPngHeights(const std::filesystem::path& path, TerrainSource& terrain)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_us* pixels = stbi_load_16(path.string().c_str(), &width, &height, &channels, 1);
            if (!pixels)
                return false;

            const bool isSquare = width == height && width > 0;
            if (isSquare)
            {
                terrain.Resolution = static_cast<uint32_t>(width);
                terrain.Heights.assign(pixels, pixels + static_cast<size_t>(width) * height);
            }
            else
            {
                LX_CORE_ERROR("{0} is {1} by {2}. A terrain's heights have to be square.", path.string(), width, height);
            }
            stbi_image_free(pixels);
            return isSquare;
        }

        bool ReadWeights(const std::filesystem::path& path, TerrainSource& terrain)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, static_cast<int>(BytesPerWeight));
            if (!pixels)
                return false;

            const bool isSquare = width == height && width > 0;
            if (isSquare)
            {
                terrain.WeightResolution = static_cast<uint32_t>(width);
                terrain.Weights.assign(pixels, pixels + static_cast<size_t>(width) * height * BytesPerWeight);
            }
            else
            {
                LX_CORE_ERROR("{0} is {1} by {2}. A terrain's weights have to be square.", path.string(), width, height);
            }
            stbi_image_free(pixels);
            return isSquare;
        }
    }

    bool TerrainSerializer::Serialize(const std::filesystem::path& filePath, const TerrainSource& terrain)
    {
        const std::string name = filePath.stem().string();
        const std::string heightsName = name + ".heights.r16";
        const std::string weightsName = name + ".weights.png";
        const bool hasLayers = !terrain.Layers.empty() && !terrain.Weights.empty();

        std::ofstream heights(filePath.parent_path() / heightsName, std::ios::binary);
        heights.write(reinterpret_cast<const char*>(terrain.Heights.data()), static_cast<std::streamsize>(terrain.Heights.size() * sizeof(uint16_t)));
        if (!heights)
        {
            LX_CORE_ERROR("Could not write the heights of terrain {0}", filePath.string());
            return false;
        }

        if (hasLayers)
        {
            const int resolution = static_cast<int>(terrain.WeightResolution);
            const std::string weightsPath = (filePath.parent_path() / weightsName).string();
            if (stbi_write_png(weightsPath.c_str(), resolution, resolution, static_cast<int>(BytesPerWeight), terrain.Weights.data(), resolution * static_cast<int>(BytesPerWeight)) == 0)
            {
                LX_CORE_ERROR("Could not write the weights of terrain {0}", filePath.string());
                return false;
            }
        }

        nlohmann::json json;
        json["Size"] = terrain.Size;
        json["Lowest"] = terrain.Lowest;
        json["Highest"] = terrain.Highest;
        json["Heights"] = heightsName;
        if (hasLayers)
        {
            json["Weights"] = weightsName;
            nlohmann::json& layers = json["Layers"] = nlohmann::json::array();
            for (const TerrainSource::Layer& layer : terrain.Layers)
                layers.push_back({ { "Material", static_cast<uint64_t>(layer.Material) }, { "TileSize", layer.TileSize } });
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
        const std::string heightsName = json.value("Heights", std::string());
        if (outTerrain.Size <= 0.0f || heightsName.empty())
        {
            LX_CORE_ERROR("Terrain {0} needs a size and its heights.", filePath.string());
            return false;
        }

        const std::filesystem::path heightsPath = filePath.parent_path() / heightsName;
        const bool hasHeights = EndsWith(heightsPath, ".png") ? ReadPngHeights(heightsPath, outTerrain) : ReadRawHeights(heightsPath, outTerrain);
        if (!hasHeights)
        {
            LX_CORE_ERROR("Could not read the heights of terrain {0} from {1}", filePath.string(), heightsPath.string());
            return false;
        }

        // Without layers, or without their weights, the terrain is drawn in its plain colours.
        const std::string weightsName = json.value("Weights", std::string());
        if (json.contains("Layers") && json["Layers"].is_array() && !weightsName.empty())
        {
            for (const nlohmann::json& layer : json["Layers"])
            {
                TerrainSource::Layer& read = outTerrain.Layers.emplace_back();
                read.Material = AssetHandle(layer.value("Material", uint64_t{ 0 }));
                read.TileSize = layer.value("TileSize", read.TileSize);
            }

            if (!ReadWeights(filePath.parent_path() / weightsName, outTerrain))
            {
                LX_CORE_ERROR("Could not read the weights of terrain {0}. It keeps its plain colours.", filePath.string());
                outTerrain.Layers.clear();
            }
        }
        return true;
    }
}
