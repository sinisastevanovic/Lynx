#pragma once
#include "Lynx/Core.h"
#include "Asset.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "MeshSpecification.h"
#include "Material.h"
#include "Lynx/Renderer/Frustum.h"

namespace Lynx
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec4 Tangent;
        glm::vec2 TexCoord;
        glm::vec4 Color;
    };

    struct Submesh
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount;
        std::shared_ptr<Material> Material;
        std::string Name;
    };

    struct SubmeshSourceData
    {
        std::vector<Vertex> Vertices;
        std::vector<uint32_t> Indices;
        std::string Name;

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

