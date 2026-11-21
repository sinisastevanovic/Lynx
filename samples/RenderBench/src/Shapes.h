#pragma once

#include <Lynx/Asset/StaticMesh.h>

#include <cstdint>
#include <vector>

// Simple shapes made in code, for scenes that need many meshes without many files.
// Every shape fits a cube of size one around the origin and faces outwards.
struct ShapeGeometry
{
    std::vector<Lynx::Vertex> Vertices;
    std::vector<uint32_t> Indices;
};

ShapeGeometry MakeBox();
ShapeGeometry MakeSphere(uint32_t segments, uint32_t rings);
ShapeGeometry MakeCylinder(uint32_t segments);
ShapeGeometry MakeTorus(uint32_t segments, uint32_t sides);
