#pragma once

#include "StaticMesh.h"

#include <span>
#include <vector>

// Turns the triangles of a mesh, as a file stores them, into the form the GPU draws fastest.
// The work is done once per mesh. Nothing here changes what a mesh looks like from close by.
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

    // Makes the coarser levels of detail of a submesh, each with about half the triangles of the one
    // before. Their indices are added behind those that are there, and every level uses the vertices as
    // they are, so a level costs indices only.
    // Returns all levels, the given triangles first. A mesh too small to gain from more keeps that one.
    //
    // So does a surface with cut-outs, which is leaves and needles. Making those simpler merges and
    // flattens them away, each too small to count as an error, and the crown of a tree gets visibly barer
    // with every level: measured on the Trees benchmark, with and without meshopt_SimplifyPrune.
    // Foliage needs a method that keeps what it covers, which is not built yet.
    std::vector<MeshLod> BuildLods(std::span<const Vertex> vertices, std::vector<uint32_t>& indices, bool hasCutOuts);

    // The vertices of a submesh in the form the GPU stores them, see PackedVertex.
    std::vector<PackedVertex> PackVertices(std::span<const Vertex> vertices);

    // A sphere around all the vertices: its centre in xyz, its radius in w. Culling tests against it.
    glm::vec4 ComputeBoundingSphere(std::span<const PackedVertex> vertices);
}
