#pragma once
#include "Lynx/Core.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Lynx
{
    class StaticMesh;

    // One kind of thing a terrain is scattered with, trees or rocks, and where it grows: a rule of the
    // terrain asset. Nothing is placed by hand. The rule says how dense the kind stands at a place,
    // from how steep and high the place is and what ground it has, and the places follow from that,
    // the same every time (Scene/TerrainScatterer).
    struct ScatterRule
    {
        std::shared_ptr<StaticMesh> Mesh;

        // How far two of them stand apart where they stand as densely as they can, in units.
        // Rules with the same spacing share their places and never put two things on one.
        float Spacing = 6.0f;

        // The share of those places that is taken where nothing below speaks against it, 0 to 1.
        float Density = 1.0f;

        // Each is made larger or smaller by a factor between these two.
        float SmallestSize = 0.85f;
        float LargestSize = 1.15f;

        // The steepest ground it grows on, in degrees from level. It thins out over the last fifth.
        float SteepestSlope = 30.0f;

        // The heights it grows between, in units above the terrain's own zero.
        float LowestHeight = -100000.0f;
        float HighestHeight = 100000.0f;

        // How well it grows on each of the terrain's layers of ground, 0 to 1. A place counts with
        // the mix of layers it has. A terrain without layers counts as the first layer everywhere.
        std::array<float, 4> LayerShares{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    // A copy of what scattering needs of one tile of a terrain: its heights and the weights of its
    // layers, with the rows of its neighbours around them as the terrain keeps them. Being a copy, it
    // can be read on another thread while the terrain is reloaded or changed.
    struct LX_API TerrainTileCopy
    {
        // Where the tile's first corner lies, in units from the terrain's, and how long its sides are.
        glm::vec2 Origin{ 0.0f };
        float Size = 0.0f;

        // How far two neighbouring heights are apart, and what the smallest and largest stored height stand for.
        float Spacing = 1.0f;
        float Lowest = 0.0f;
        float Highest = 0.0f;

        // How many rows of the neighbours lie around the tile's own, and the texels a side with them.
        uint32_t Border = 1;
        uint32_t HeightTexels = 0;
        std::vector<uint16_t> Heights;

        // Four bytes a place. No texels for a terrain without layers.
        uint32_t WeightTexels = 0;
        std::vector<uint8_t> Weights;

        // The height of the ground at a place given in units from the tile's first corner, on the
        // triangles the ground is drawn with, like Terrain::GetHeightAtPlace. The place may lie up to
        // one spacing outside the tile.
        float HeightAt(float x, float z) const;

        // Which way the ground faces there, of length one.
        glm::vec3 NormalAt(float x, float z) const;

        // How much of each layer shows there, 0 to 1 each.
        std::array<float, 4> LayersAt(float x, float z) const;
    };
}
