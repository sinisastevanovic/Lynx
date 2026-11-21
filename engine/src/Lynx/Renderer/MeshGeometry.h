#pragma once

#include <cstdint>
#include <memory>

namespace Lynx
{
    // Where the geometry pool has the vertices and indices of one submesh, see Vulkan/GeometryPool.
    // A copy refers to the same place, which is given back when the last copy goes away, and then only
    // after the frames that may still draw from it are done.
    class MeshGeometry
    {
    public:
        MeshGeometry() = default;

        // False for a submesh the pool had no room for. Such a submesh cannot be drawn.
        explicit operator bool() const { return m_Place != nullptr; }

        // The first of the submesh's indices in the pool's index buffer.
        uint32_t FirstIndex() const { return m_FirstIndex; }

        // What is added to each of the submesh's indices to reach its vertices in the pool's vertex buffer.
        int32_t VertexOffset() const { return m_VertexOffset; }

    private:
        friend class GeometryPool;

        uint32_t m_FirstIndex = 0;
        int32_t m_VertexOffset = 0;

        // Gives the place back to the pool when the last copy lets go of it.
        std::shared_ptr<void> m_Place;
    };
}
