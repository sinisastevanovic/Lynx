#include "MeshCooking.h"

#include <meshoptimizer.h>

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
