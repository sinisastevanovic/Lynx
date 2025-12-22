#pragma once
#include "Lynx/Core.h"
#include "Asset.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace Lynx
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec2 TexCoord;
    };
    
    class LX_API StaticMesh : public Asset
    {
    public:
        StaticMesh(nvrhi::DeviceHandle device, const std::string& filepath);
        StaticMesh(nvrhi::DeviceHandle device, std::vector<Vertex> vertices, std::vector<uint32_t> indices);
        virtual ~StaticMesh() = default;

        static AssetType GetStaticType() { return AssetType::StaticMesh; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        nvrhi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer; }
        nvrhi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer; }
        uint32_t GetIndexCount() const { return m_IndexCount; }

        // TODO: Temp!
        AssetHandle GetTexture() const { return m_TextureAssetHandle; }
        void SetTexture(AssetHandle textureHandle) { m_TextureAssetHandle = textureHandle; }

    private:
        void UploadBuffers(nvrhi::DeviceHandle device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        
    private:
        nvrhi::BufferHandle m_VertexBuffer;
        nvrhi::BufferHandle m_IndexBuffer;
        uint32_t m_IndexCount = 0;
        AssetHandle m_TextureAssetHandle = AssetHandle::Null();
    };
}

