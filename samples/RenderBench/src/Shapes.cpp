#include "Shapes.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <utility>

using Lynx::Vertex;

namespace
{
    uint32_t AddVertex(ShapeGeometry& geometry, const glm::vec3& position, const glm::vec3& normal, const glm::vec3& tangent, const glm::vec2& texCoord)
    {
        Vertex vertex;
        vertex.Position = position;
        vertex.Normal = normal;
        vertex.Tangent = glm::vec4(tangent, 1.0f);
        vertex.TexCoord = texCoord;
        vertex.Color = glm::vec4(1.0f);
        geometry.Vertices.push_back(vertex);
        return static_cast<uint32_t>(geometry.Vertices.size() - 1);
    }

    // Adds a triangle so that it faces the way the normals of its corners point, whatever order they come in.
    // A triangle without an area is left out: the rings at the poles of a sphere produce those.
    void AddTriangle(ShapeGeometry& geometry, uint32_t a, uint32_t b, uint32_t c)
    {
        const Vertex& vertexA = geometry.Vertices[a];
        const Vertex& vertexB = geometry.Vertices[b];
        const Vertex& vertexC = geometry.Vertices[c];

        const glm::vec3 facing = glm::cross(vertexB.Position - vertexA.Position, vertexC.Position - vertexA.Position);
        if (glm::dot(facing, facing) < 1e-12f)
            return;

        if (glm::dot(facing, vertexA.Normal + vertexB.Normal + vertexC.Normal) < 0.0f)
            std::swap(b, c);

        geometry.Indices.push_back(a);
        geometry.Indices.push_back(b);
        geometry.Indices.push_back(c);
    }

    void AddQuad(ShapeGeometry& geometry, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    {
        AddTriangle(geometry, a, b, c);
        AddTriangle(geometry, a, c, d);
    }

    // Connects vertices that were added row by row, each row having columns + 1 of them.
    void AddGrid(ShapeGeometry& geometry, uint32_t firstVertex, uint32_t columns, uint32_t rows)
    {
        const uint32_t stride = columns + 1;
        for (uint32_t row = 0; row < rows; ++row)
        {
            for (uint32_t column = 0; column < columns; ++column)
            {
                const uint32_t corner = firstVertex + row * stride + column;
                AddQuad(geometry, corner, corner + 1, corner + stride + 1, corner + stride);
            }
        }
    }
}

ShapeGeometry MakeBox()
{
    ShapeGeometry geometry;

    // The normal of each face and the direction its texture runs in.
    const std::pair<glm::vec3, glm::vec3> faces[] = {
        { { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f } },
    };

    for (const auto& [normal, tangent] : faces)
    {
        const glm::vec3 bitangent = glm::cross(normal, tangent);
        const glm::vec3 centre = 0.5f * normal;
        const uint32_t a = AddVertex(geometry, centre - 0.5f * tangent - 0.5f * bitangent, normal, tangent, { 0.0f, 1.0f });
        const uint32_t b = AddVertex(geometry, centre + 0.5f * tangent - 0.5f * bitangent, normal, tangent, { 1.0f, 1.0f });
        const uint32_t c = AddVertex(geometry, centre + 0.5f * tangent + 0.5f * bitangent, normal, tangent, { 1.0f, 0.0f });
        const uint32_t d = AddVertex(geometry, centre - 0.5f * tangent + 0.5f * bitangent, normal, tangent, { 0.0f, 0.0f });
        AddQuad(geometry, a, b, c, d);
    }

    return geometry;
}

ShapeGeometry MakeSphere(uint32_t segments, uint32_t rings)
{
    ShapeGeometry geometry;

    for (uint32_t ring = 0; ring <= rings; ++ring)
    {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float polar = v * glm::pi<float>();

        for (uint32_t segment = 0; segment <= segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float around = u * glm::two_pi<float>();

            const glm::vec3 normal(std::sin(polar) * std::cos(around), std::cos(polar), std::sin(polar) * std::sin(around));
            const glm::vec3 tangent(-std::sin(around), 0.0f, std::cos(around));
            AddVertex(geometry, 0.5f * normal, normal, tangent, { u, v });
        }
    }

    AddGrid(geometry, 0, segments, rings);
    return geometry;
}

ShapeGeometry MakeCylinder(uint32_t segments)
{
    ShapeGeometry geometry;

    // The side: a ring at the top and one at the bottom.
    for (uint32_t row = 0; row < 2; ++row)
    {
        const float height = row == 0 ? 0.5f : -0.5f;
        for (uint32_t segment = 0; segment <= segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float around = u * glm::two_pi<float>();

            const glm::vec3 normal(std::cos(around), 0.0f, std::sin(around));
            const glm::vec3 tangent(-std::sin(around), 0.0f, std::cos(around));
            AddVertex(geometry, glm::vec3(0.5f * normal.x, height, 0.5f * normal.z), normal, tangent, { u, static_cast<float>(row) });
        }
    }
    AddGrid(geometry, 0, segments, 1);

    // The two lids, each a fan around its centre.
    for (const float direction : { 1.0f, -1.0f })
    {
        const glm::vec3 normal(0.0f, direction, 0.0f);
        const glm::vec3 tangent(1.0f, 0.0f, 0.0f);
        const uint32_t centre = AddVertex(geometry, 0.5f * normal, normal, tangent, { 0.5f, 0.5f });

        for (uint32_t segment = 0; segment <= segments; ++segment)
        {
            const float around = static_cast<float>(segment) / static_cast<float>(segments) * glm::two_pi<float>();
            const glm::vec2 onCircle(std::cos(around), std::sin(around));
            const uint32_t rim = AddVertex(geometry, glm::vec3(0.5f * onCircle.x, 0.5f * direction, 0.5f * onCircle.y), normal, tangent, 0.5f + 0.5f * onCircle);
            if (segment > 0)
                AddTriangle(geometry, centre, rim - 1, rim);
        }
    }

    return geometry;
}

ShapeGeometry MakeTorus(uint32_t segments, uint32_t sides)
{
    ShapeGeometry geometry;

    // The ring lies flat. Ring and tube together reach the sides of the unit cube.
    const float ringRadius = 0.35f;
    const float tubeRadius = 0.15f;

    for (uint32_t side = 0; side <= sides; ++side)
    {
        const float v = static_cast<float>(side) / static_cast<float>(sides);
        const float aroundTube = v * glm::two_pi<float>();

        for (uint32_t segment = 0; segment <= segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float aroundRing = u * glm::two_pi<float>();

            const glm::vec3 outwards(std::cos(aroundRing), 0.0f, std::sin(aroundRing));
            const glm::vec3 normal = std::cos(aroundTube) * outwards + glm::vec3(0.0f, std::sin(aroundTube), 0.0f);
            const glm::vec3 tangent(-std::sin(aroundRing), 0.0f, std::cos(aroundRing));
            AddVertex(geometry, ringRadius * outwards + tubeRadius * normal, normal, tangent, { u, v });
        }
    }

    AddGrid(geometry, 0, segments, sides);
    return geometry;
}
