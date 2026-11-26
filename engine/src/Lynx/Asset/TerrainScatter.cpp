#include "TerrainScatter.h"

#include <algorithm>
#include <cmath>

namespace Lynx
{
    float TerrainTileCopy::HeightAt(float x, float z) const
    {
        if (Heights.empty() || HeightTexels < 2)
            return Lowest;

        const auto stored = [this](int32_t texelX, int32_t texelZ)
        {
            const int32_t last = static_cast<int32_t>(HeightTexels) - 1;
            const size_t index = static_cast<size_t>(std::clamp(texelZ, 0, last)) * HeightTexels + static_cast<size_t>(std::clamp(texelX, 0, last));
            return Lowest + (Highest - Lowest) * (static_cast<float>(Heights[index]) / 65535.0f);
        };

        // In steps of the heights, counted from the first texel, which lies in the border.
        const float inStepsX = x / Spacing + static_cast<float>(Border);
        const float inStepsZ = z / Spacing + static_cast<float>(Border);
        const float quadX = std::floor(inStepsX);
        const float quadZ = std::floor(inStepsZ);
        const float alongX = inStepsX - quadX;
        const float alongZ = inStepsZ - quadZ;
        const int32_t texelX = static_cast<int32_t>(quadX);
        const int32_t texelZ = static_cast<int32_t>(quadZ);

        // A quad is two triangles, split from its first corner to the one opposite, as everywhere.
        const float first = stored(texelX, texelZ);
        const float opposite = stored(texelX + 1, texelZ + 1);
        if (alongZ >= alongX)
        {
            const float alongZOnly = stored(texelX, texelZ + 1);
            return first + alongZ * (alongZOnly - first) + alongX * (opposite - alongZOnly);
        }
        const float alongXOnly = stored(texelX + 1, texelZ);
        return first + alongX * (alongXOnly - first) + alongZ * (opposite - alongXOnly);
    }

    glm::vec3 TerrainTileCopy::NormalAt(float x, float z) const
    {
        // From the heights a step to either side, which the border has for places at the tile's edge.
        const float towardsX = HeightAt(x + Spacing, z) - HeightAt(x - Spacing, z);
        const float towardsZ = HeightAt(x, z + Spacing) - HeightAt(x, z - Spacing);
        return glm::normalize(glm::vec3(-towardsX, 2.0f * Spacing, -towardsZ));
    }

    std::array<float, 4> TerrainTileCopy::LayersAt(float x, float z) const
    {
        if (WeightTexels <= 2 * Border || Weights.size() < static_cast<size_t>(WeightTexels) * WeightTexels * 4 || Size <= 0.0f)
            return { 1.0f, 0.0f, 0.0f, 0.0f };

        // The weights cover the tile in as many places as there are texels without the border.
        const float places = static_cast<float>(WeightTexels - 2 * Border);
        const int32_t last = static_cast<int32_t>(WeightTexels) - 1;
        const int32_t texelX = std::clamp(static_cast<int32_t>(std::floor(x / Size * places)) + static_cast<int32_t>(Border), 0, last);
        const int32_t texelZ = std::clamp(static_cast<int32_t>(std::floor(z / Size * places)) + static_cast<int32_t>(Border), 0, last);
        const uint8_t* weights = &Weights[(static_cast<size_t>(texelZ) * WeightTexels + static_cast<size_t>(texelX)) * 4];
        return { weights[0] / 255.0f, weights[1] / 255.0f, weights[2] / 255.0f, weights[3] / 255.0f };
    }
}
