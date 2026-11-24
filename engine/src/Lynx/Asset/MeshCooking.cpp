#include "MeshCooking.h"

#include <meshoptimizer.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Lynx
{
    void OptimizeDrawOrder(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
    {
        if (vertices.empty() || indices.size() < 3)
            return;

        // The steps follow the order meshoptimizer recommends: each one keeps what the one before it gained.
        std::vector<uint32_t> remap(vertices.size());
        const size_t uniqueVertexCount = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Vertex));

        std::vector<Vertex> uniqueVertices(uniqueVertexCount);
        meshopt_remapVertexBuffer(uniqueVertices.data(), vertices.data(), vertices.size(), sizeof(Vertex), remap.data());
        meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());

        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), uniqueVertexCount);

        // How much worse the order may get for the vertex cache in exchange for hiding more pixels: 5 %.
        const float allowedCacheLoss = 1.05f;
        meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &uniqueVertices[0].Position.x, uniqueVertexCount, sizeof(Vertex), allowedCacheLoss);

        const size_t usedVertexCount = meshopt_optimizeVertexFetch(uniqueVertices.data(), indices.data(), indices.size(), uniqueVertices.data(), uniqueVertexCount, sizeof(Vertex));
        uniqueVertices.resize(usedVertexCount);
        vertices = std::move(uniqueVertices);
    }

    namespace
    {
        // Below this many triangles, a coarser level saves too little to be worth draw commands of its own.
        constexpr size_t FewestTrianglesForLods = 1024;

        // No level is made coarser than this.
        constexpr size_t FewestTrianglesOfALod = 128;

        // A level that keeps more than this share of the triangles of the one before is not kept: the
        // mesh cannot be made simpler in a way that pays.
        constexpr float LargestShareOfTriangles = 0.75f;
    }

    std::vector<MeshLod> BuildLods(std::span<const Vertex> vertices, std::vector<uint32_t>& indices, bool hasCutOuts)
    {
        std::vector<MeshLod> lods;
        lods.push_back({ 0, static_cast<uint32_t>(indices.size()), 0.0f });
        if (hasCutOuts || indices.size() / 3 < FewestTrianglesForLods)
            return lods;

        const float* positions = &vertices.front().Position.x;

        // The normal counts in what removing a vertex costs, so that a level keeps its shading and not
        // only its shape.
        const float* normals = &vertices.front().Normal.x;
        const float normalWeights[3] = { 0.5f, 0.5f, 0.5f };

        const unsigned int options = 0;

        // The simplifier counts error relative to the size of the mesh. This turns it into the mesh's units.
        const float errorScale = meshopt_simplifyScale(positions, vertices.size(), sizeof(Vertex));

        // Each level is made from the one before, which is much faster than from the original and
        // adds up the errors: the sum is at least as large as the true distance to the original.
        std::vector<uint32_t> previous(indices);
        std::vector<uint32_t> level;
        float error = 0.0f;
        while (lods.size() < MeshLod::MaxCount && previous.size() / 3 > FewestTrianglesOfALod)
        {
            const size_t targetIndexCount = std::max(previous.size() / 6, FewestTrianglesOfALod) * 3;

            // Any error is allowed: how coarse a level may be is decided when it is drawn, from its error.
            float relativeError = 0.0f;
            level.resize(previous.size());
            const size_t indexCount = meshopt_simplifyWithAttributes(level.data(), previous.data(), previous.size(), positions, vertices.size(), sizeof(Vertex),
                normals, sizeof(Vertex), normalWeights, 3, nullptr, targetIndexCount, 1.0f, options, &relativeError);
            const bool isTooCoarse = indexCount / 3 < FewestTrianglesOfALod;
            const bool savesTooLittle = static_cast<float>(indexCount) > static_cast<float>(previous.size()) * LargestShareOfTriangles;
            if (isTooCoarse || savesTooLittle)
                break;

            level.resize(indexCount);
            meshopt_optimizeVertexCache(level.data(), level.data(), level.size(), vertices.size());

            error += relativeError * errorScale;
            lods.push_back({ static_cast<uint32_t>(indices.size()), static_cast<uint32_t>(level.size()), error });
            indices.insert(indices.end(), level.begin(), level.end());
            previous.swap(level);
        }

        return lods;
    }

    namespace
    {
        // With fewer separate pieces than this, a submesh is not foliage: taking pieces away would show.
        constexpr size_t FewestLeaves = 256;

        // A piece this many times larger than the usual one is not a leaf but something leaves hang on,
        // such as the stem of a twig. It is always drawn and never made larger: grown like a leaf, it
        // stuck out of the crown as a spike many metres long.
        constexpr float LargestLeafInLeafSizes = 4.0f;

        // Mixes the bits of a number so that neighbouring numbers give unrelated results.
        uint32_t Hash(uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }
    }

    Foliage BuildFoliageLods(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
    {
        const size_t triangleCount = indices.size() / 3;
        if (triangleCount < FewestTrianglesForLods)
            return {};

        // Vertices at the same position count as one here: a leaf is often cut along its texture's seam.
        std::vector<glm::vec3> positions(vertices.size());
        for (size_t index = 0; index < vertices.size(); ++index)
            positions[index] = vertices[index].Position;
        std::vector<uint32_t> atPosition(vertices.size());
        meshopt_generateVertexRemap(atPosition.data(), nullptr, vertices.size(), positions.data(), vertices.size(), sizeof(glm::vec3));

        // Which positions hang together through triangles. Each group is a piece, called a leaf from here on.
        std::vector<uint32_t> group(vertices.size());
        for (uint32_t index = 0; index < group.size(); ++index)
            group[index] = index;
        const auto groupOf = [&group](uint32_t position)
        {
            while (group[position] != position)
            {
                group[position] = group[group[position]];
                position = group[position];
            }
            return position;
        };
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
        {
            const uint32_t first = groupOf(atPosition[indices[triangle * 3]]);
            group[groupOf(atPosition[indices[triangle * 3 + 1]])] = first;
            group[groupOf(atPosition[indices[triangle * 3 + 2]])] = first;
        }

        struct LeafBounds
        {
            glm::vec3 Smallest{ std::numeric_limits<float>::max() };
            glm::vec3 Largest{ std::numeric_limits<float>::lowest() };
            uint32_t TriangleCount = 0;

            // Where its triangles start in the new order, counted in triangles.
            uint32_t FirstTriangle = 0;

            float Size() const { return glm::length(Largest - Smallest); }
        };
        constexpr uint32_t NoLeaf = ~0u;
        std::vector<uint32_t> leafOfGroup(vertices.size(), NoLeaf);
        std::vector<uint32_t> leafOfTriangle(triangleCount);
        std::vector<LeafBounds> leaves;
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
        {
            uint32_t& leaf = leafOfGroup[groupOf(atPosition[indices[triangle * 3]])];
            if (leaf == NoLeaf)
            {
                leaf = static_cast<uint32_t>(leaves.size());
                leaves.emplace_back();
            }

            leafOfTriangle[triangle] = leaf;
            leaves[leaf].TriangleCount++;
            for (size_t corner = 0; corner < 3; ++corner)
            {
                const glm::vec3& position = positions[indices[triangle * 3 + corner]];
                leaves[leaf].Smallest = glm::min(leaves[leaf].Smallest, position);
                leaves[leaf].Largest = glm::max(leaves[leaf].Largest, position);
            }
        }
        if (leaves.size() < FewestLeaves)
            return {};

        // The size of the usual leaf: the one in the middle when they are lined up by size.
        std::vector<float> sizes(leaves.size());
        for (size_t leaf = 0; leaf < leaves.size(); ++leaf)
            sizes[leaf] = leaves[leaf].Size();
        std::ranges::nth_element(sizes, sizes.begin() + sizes.size() / 2);
        const float leafSize = sizes[sizes.size() / 2];
        const auto isLeaf = [&](uint32_t piece) { return leaves[piece].Size() <= leafSize * LargestLeafInLeafSizes; };

        // What is always drawn comes first. The leaves follow in a random order that is the same every
        // time the mesh is cooked. Any first part of it is spread evenly over the crown, which is what
        // lets a level be "the first part of the list".
        std::vector<uint32_t> order(leaves.size());
        for (uint32_t index = 0; index < order.size(); ++index)
            order[index] = index;
        const auto firstLeaf = std::ranges::stable_partition(order, [&](uint32_t piece) { return !isLeaf(piece); }).begin();
        std::ranges::sort(firstLeaf, order.end(), [](uint32_t a, uint32_t b) { return std::pair(Hash(a), a) < std::pair(Hash(b), b); });
        if (static_cast<size_t>(order.end() - firstLeaf) < FewestLeaves)
            return {};

        uint32_t trianglesSoFar = 0;
        uint32_t alwaysDrawnTriangles = 0;
        for (auto piece = order.begin(); piece != order.end(); ++piece)
        {
            if (piece == firstLeaf)
                alwaysDrawnTriangles = trianglesSoFar;
            leaves[*piece].FirstTriangle = trianglesSoFar;
            trianglesSoFar += leaves[*piece].TriangleCount;
        }
        const float leafTriangles = static_cast<float>(triangleCount - alwaysDrawnTriangles);

        // Where a piece ends in the list of leaves, from just above 0 to 1. Zero for what is always drawn.
        const auto rankOf = [&](const LeafBounds& piece)
        {
            const uint32_t end = piece.FirstTriangle + piece.TriangleCount;
            return end <= alwaysDrawnTriangles ? 0.0f : static_cast<float>(end - alwaysDrawnTriangles) / leafTriangles;
        };

        // The triangles of a leaf stay in the order they had, which was good for the vertex cache.
        std::vector<uint32_t> ordered(indices.size());
        std::vector<uint32_t> written(leaves.size(), 0);
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
        {
            const uint32_t leaf = leafOfTriangle[triangle];
            const size_t target = static_cast<size_t>(leaves[leaf].FirstTriangle + written[leaf]++) * 3;
            for (size_t corner = 0; corner < 3; ++corner)
                ordered[target + corner] = indices[triangle * 3 + corner];
        }

        // The vertices follow the triangles, so that a level reads the first part of the vertices too.
        std::vector<uint32_t> newPlace(vertices.size());
        const size_t usedVertexCount = meshopt_optimizeVertexFetchRemap(newPlace.data(), ordered.data(), ordered.size(), vertices.size());
        std::vector<Vertex> orderedVertices(usedVertexCount);
        meshopt_remapVertexBuffer(orderedVertices.data(), vertices.data(), vertices.size(), sizeof(Vertex), newPlace.data());

        Foliage foliage;
        foliage.LeafSize = leafSize;
        foliage.Leaves.resize(usedVertexCount);
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
        {
            const LeafBounds& leaf = leaves[leafOfTriangle[triangle]];
            for (size_t corner = 0; corner < 3; ++corner)
            {
                Leaf& ofVertex = foliage.Leaves[newPlace[indices[triangle * 3 + corner]]];
                ofVertex.Centre = 0.5f * (leaf.Smallest + leaf.Largest);
                ofVertex.Rank = rankOf(leaf);
            }
        }

        meshopt_remapIndexBuffer(ordered.data(), ordered.data(), ordered.size(), newPlace.data());
        indices = std::move(ordered);
        vertices = std::move(orderedVertices);

        // Level k draws what is always drawn and the leaves that end within the first 1 / 2^k of their
        // list, and draws those larger by the square root of 2^k. Its error is the size a leaf then has:
        // a level may be drawn while that is small on screen.
        foliage.Lods.push_back({ 0, static_cast<uint32_t>(indices.size()), 0.0f });
        size_t nextLeaf = order.size();
        for (size_t level = 1; level < MeshLod::MaxCount; ++level)
        {
            const float share = std::exp2(-static_cast<float>(level));
            while (nextLeaf > 0 && rankOf(leaves[order[nextLeaf - 1]]) > share)
                --nextLeaf;

            const size_t keptTriangles = nextLeaf == 0 ? 0 : leaves[order[nextLeaf - 1]].FirstTriangle + leaves[order[nextLeaf - 1]].TriangleCount;
            if (keptTriangles < alwaysDrawnTriangles + FewestTrianglesOfALod)
                break;

            foliage.Lods.push_back({ 0, static_cast<uint32_t>(keptTriangles * 3), foliage.LeafSize * std::sqrt(std::exp2(static_cast<float>(level))) });
        }

        return foliage;
    }

    std::vector<LeafVertex> PackLeaves(std::span<const Leaf> leaves, const glm::vec4& boundingSphere)
    {
        const float radius = std::max(boundingSphere.w, 0.000001f);

        std::vector<LeafVertex> packed;
        packed.reserve(leaves.size());
        for (const Leaf& leaf : leaves)
        {
            const glm::vec3 fromCentre = (leaf.Centre - glm::vec3(boundingSphere)) / radius;

            // The rank is rounded up, so that a leaf never counts as part of a level it does not fit in,
            // and only what is always drawn has a rank of zero.
            LeafVertex& out = packed.emplace_back();
            for (int axis = 0; axis < 3; ++axis)
                out.Pivot[axis] = static_cast<int16_t>(meshopt_quantizeSnorm(fromCentre[axis], 16));
            out.Rank = static_cast<uint16_t>(std::min(std::ceil(leaf.Rank * 65535.0f), 65535.0f));
        }
        return packed;
    }

    namespace
    {
        // Three numbers from -1 to 1 in 10 bits each, and one that is -1, 0 or 1 in the 2 bits above them.
        uint32_t PackDirection(const glm::vec3& direction, int top)
        {
            const auto tenBits = [](float value) { return static_cast<uint32_t>(meshopt_quantizeSnorm(value, 10)) & 0x3FFu; };
            return tenBits(direction.x) | (tenBits(direction.y) << 10) | (tenBits(direction.z) << 20) | ((static_cast<uint32_t>(top) & 0x3u) << 30);
        }
    }

    std::vector<PackedVertex> PackVertices(std::span<const Vertex> vertices)
    {
        std::vector<PackedVertex> packed;
        packed.reserve(vertices.size());

        for (const Vertex& vertex : vertices)
        {
            PackedVertex& out = packed.emplace_back();
            out.Position = vertex.Position;
            out.Normal = PackDirection(vertex.Normal, 0);
            out.Tangent = PackDirection(glm::vec3(vertex.Tangent), vertex.Tangent.w < 0.0f ? -1 : 1);
            out.TexCoord = vertex.TexCoord;
        }

        return packed;
    }

    glm::vec4 ComputeBoundingSphere(std::span<const PackedVertex> vertices)
    {
        if (vertices.empty())
            return glm::vec4(0.0f);

        const meshopt_Bounds bounds = meshopt_computeSphereBounds(&vertices.front().Position.x, vertices.size(), sizeof(PackedVertex), nullptr, 0);
        return glm::vec4(bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius);
    }
}
