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
}
