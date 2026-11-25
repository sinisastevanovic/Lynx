#pragma once
#include "Lynx/Core.h"
#include "Asset.h"
#include <glm/glm.hpp>

#include "MeshSpecification.h"
#include "Material.h"
#include "Lynx/Renderer/Frustum.h"
#include "Lynx/Renderer/MeshGeometry.h"
#include "Lynx/Renderer/MeshVertex.h"

namespace Lynx
{
    class GpuBuffer;

    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec4 Tangent;
        glm::vec2 TexCoord;
        glm::vec4 Color;
    };

    // One level of detail of a submesh: the same surface in fewer triangles, for when it is small on screen.
    // Every level is a run of the submesh's indices over the same vertices.
    struct MeshLod
    {
        // More levels than this are not made, so that what is kept per level stays small.
        static constexpr size_t MaxCount = 12;

        // Counted from the first index of the submesh.
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;

        // How far the level's surface is from the original's at most, in the mesh's own units.
        // Zero for the level the file holds.
        float Error = 0.0f;
    };

    // What a vertex of foliage knows about the leaf it is part of, so that a vertex shader can make the
    // leaf larger or let it shrink away, see BuildFoliageLods. Shaders read it as LeafVertex in Mesh.slang.
    struct LeafVertex
    {
        // The centre of the leaf, counted from the centre of the sphere around the submesh in units of
        // its radius: three numbers from -1 to 1 in 16 bits each.
        int16_t Pivot[3];

        // The last level of detail the leaf is drawn in, and the level from which on it is made larger,
        // both in eighths of a level. A leaf of the usual size grows from level 0 on. A larger piece is
        // left as it is until the leaves around it have grown to its size, see BuildFoliageLods.
        uint8_t LastLevel;
        uint8_t FirstLevel;
    };

    struct Submesh
    {
        // Where the renderer has the vertices and indices.
        MeshGeometry Geometry;

        // A sphere around the submesh, in the mesh's own space: its centre in xyz, its radius in w.
        glm::vec4 BoundingSphere{ 0.0f };

        // Of the finest level.
        uint32_t IndexCount;

        // The finest first, each one coarser than the one before. Never empty for a submesh that can be drawn.
        std::vector<MeshLod> Lods;

        // For foliage: one LeafVertex per vertex, on the GPU, and the size of an average leaf in the
        // mesh's units. Null and zero for everything else.
        std::shared_ptr<const GpuBuffer> LeafData;
        float LeafSize = 0.0f;
        std::shared_ptr<Material> Material;
        std::string Name;
    };

    struct SubmeshSourceData
    {
        // What the file holds. Emptied once the vertices are packed.
        std::vector<Vertex> Vertices;

        // The indices of all levels of detail, one level after the other.
        std::vector<uint32_t> Indices;
        std::vector<MeshLod> Lods;

        // For foliage, see Submesh. Empty and zero for everything else.
        std::vector<LeafVertex> Leaves;
        float LeafSize = 0.0f;
        std::string Name;

        // The vertices as the GPU stores them, made where the file is read so that it happens off the main thread.
        std::vector<PackedVertex> PackedVertices;
        glm::vec4 BoundingSphere{ 0.0f };

        // TODO: why not store a material here? 
        struct MatDef
        {
            glm::vec4 AlbedoColor = {1, 1, 1, 1};
            float Metallic = 1.0f;
            float Roughness = 1.0f;
            glm::vec3 EmissiveColor = { 0, 0, 0 };
            float EmissiveStrength = 1.0f;
            AlphaMode Mode = AlphaMode::Opaque;
            float AlphaCutoff = 0.5f;
            bool TwoSided = false;

            std::string AlbedoPath;
            std::string NormalPath;
            std::string MetallicRoughnessPath;
            std::string EmissivePath;
        } MaterialData;
    };
    
    class LX_API StaticMesh : public Asset
    {
    public:
        StaticMesh(const std::string& filepath);
        StaticMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
        virtual ~StaticMesh() = default;

        static AssetType GetStaticType() { return AssetType::StaticMesh; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        const StaticMeshSpecification& GetSpecification() const { return m_Specification; }
        const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }
        const AABB& GetBounds() const { return m_Bounds; }

        virtual bool Reload() override;

        virtual bool LoadSourceData() override;
        virtual bool CreateRenderResources() override;

    private:
        std::vector<Submesh> m_Submeshes;
        StaticMeshSpecification m_Specification;
        AABB m_Bounds;

        std::vector<SubmeshSourceData> m_SourceData;
    };
}

