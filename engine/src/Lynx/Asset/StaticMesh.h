#pragma once
#include "Lynx/Core.h"
#include "Asset.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "MeshSpecification.h"
#include "Material.h"

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

        virtual bool Reload() override;

    private:
        void LoadAndUpload();
        
    private:
        std::vector<Submesh> m_Submeshes;
        StaticMeshSpecification m_Specification;
        
        std::string m_FilePath;
    };
}

