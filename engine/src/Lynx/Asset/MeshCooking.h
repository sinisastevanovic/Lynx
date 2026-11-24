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
    // Foliage gets its levels from BuildFoliageLods instead.
    std::vector<MeshLod> BuildLods(std::span<const Vertex> vertices, std::vector<uint32_t>& indices, bool hasCutOuts);

    // What a vertex of foliage knows about its leaf before it is packed, see LeafVertex.
    struct Leaf
    {
        glm::vec3 Centre{ 0.0f };

        // Where the leaf ends in the list of leaves, from just above 0 to 1.
        // Zero for a piece too large to be a leaf, such as the stem of a twig: it is always drawn.
        float Rank = 0.0f;
    };

    struct Foliage
    {
        // Empty if the submesh is not foliage: not made of enough separate pieces.
        std::vector<MeshLod> Lods;

        // One per vertex.
        std::vector<Leaf> Leaves;

        // The size of an average leaf, in the mesh's units.
        float LeafSize = 0.0f;
    };

    // The levels of detail of foliage: a submesh made of many pieces that hang together with nothing
    // else, which are leaves, needles, twigs or blades of grass.
    //
    // A coarser level draws fewer leaves and draws them larger, so that the crown covers what it covered
    // before ("Stochastic Pruning", Cook and Halstead, Pixar 2007). The leaves are put in a random order
    // here, so a level is the first part of the list of triangles and costs nothing to store. Every level
    // keeps half the leaves of the one before. A vertex shader grows the leaves that are left about
    // their centres, which is why every vertex gets the centre of its leaf.
    //
    // Reorders the triangles and the vertices.
    Foliage BuildFoliageLods(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

    // The leaves in the form the GPU reads them, for a submesh inside the given sphere.
    std::vector<LeafVertex> PackLeaves(std::span<const Leaf> leaves, const glm::vec4& boundingSphere);

    // The vertices of a submesh in the form the GPU stores them, see PackedVertex.
    std::vector<PackedVertex> PackVertices(std::span<const Vertex> vertices);

    // A sphere around all the vertices: its centre in xyz, its radius in w. Culling tests against it.
    glm::vec4 ComputeBoundingSphere(std::span<const PackedVertex> vertices);
}
