#pragma once

#include "StaticMesh.h"

#include <vector>

// Turns the triangles of a mesh, as a file stores them, into the form the GPU draws fastest.
// The work is done once per mesh. Nothing here changes what a mesh looks like.
namespace Lynx
{
    // Puts the triangles and the vertices of a submesh into a better order:
    // - Vertices that are exactly alike become one.
    // - Triangles that share vertices follow each other, so the GPU transforms a vertex once and
    //   finds it again for the next triangles instead of transforming it anew.
    // - Among orders that are equally good for that, the one is taken that draws the outside of the
    //   mesh first, so fewer pixels are shaded that end up hidden.
    // - Vertices are stored in the order the triangles use them, so drawing reads memory front to back.
    void OptimizeDrawOrder(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
}
