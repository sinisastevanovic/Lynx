#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Lynx
{
    // A mesh vertex as the GPU stores it: 28 bytes, where the vertex a mesh is loaded with has 64.
    // Drawing a mesh is to a good part reading its vertices, so a smaller vertex is a faster one.
    //
    // The pipelines of the mesh passes describe this layout (MeshVertexLayout in Passes/MeshDraw.h),
    // and the GPU unpacks it: a shader gets plain numbers.
    struct PackedVertex
    {
        glm::vec3 Position;

        // Three numbers from -1 to 1 in 10 bits each, x lowest. The top 2 bits are unused.
        uint32_t Normal;

        // The same for the direction. The top 2 bits hold 1 or -1: which way the third direction of the
        // surface points, see Vertex::Tangent.
        uint32_t Tangent;

        // Kept exact. In 16 bits each, a texture that repeats many times across a mesh would shift by
        // a texel or more: measured on Sponza's roof, and worse on anything tiled further, like terrain.
        glm::vec2 TexCoord;
    };
}
