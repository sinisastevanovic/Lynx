#pragma once

#include "Lynx/Core.h"
#include "Lynx/UI/Core/UICanvas.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace Lynx
{
    class Material;
    class Texture;
    
    struct UIVertex
    {
        glm::vec3 Position;
        glm::vec2 UV;
        glm::vec4 Color;
    };

    struct UIBatch
    {
        std::shared_ptr<Material> Material;
        nvrhi::TextureHandle Texture;

        uint32_t IndexStart;
        uint32_t IndexCount;
    };

    class LX_API UIBatcher
    {
    public:
        UIBatcher(nvrhi::DeviceHandle device);
        ~UIBatcher() = default;

        void Begin();
        void Submit(std::shared_ptr<UICanvas> canvas);
        void Upload(nvrhi::CommandListHandle commandList);

        nvrhi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer; }
        nvrhi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer; }
        const std::vector<UIBatch>& GetBatches() const { return m_Batches; }
        bool HasData() const { return !m_Batches.empty(); }

        void DrawRect(const UIRect& rect, const glm::vec4& color, std::shared_ptr<Material> material, std::shared_ptr<Texture> textureOverride);

    private:
        void TraverseAndCollect(std::shared_ptr<UIElement> element, const glm::vec2& parentAbsPos, float scale);
        void AddQuad(const UIRect& rect, const glm::vec4& color);
        void ResizeBuffers(size_t numVerts, size_t numIndices);

    private:
        nvrhi::DeviceHandle m_Device;

        std::vector<UIVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<UIBatch> m_Batches;

        nvrhi::BufferHandle m_VertexBuffer;
        nvrhi::BufferHandle m_IndexBuffer;
        size_t m_VertexBufferSize = 0;
        size_t m_IndexBufferSize = 0;

        std::shared_ptr<Material> m_CurrentMaterial = nullptr;
        nvrhi::TextureHandle m_CurrentTexture = nullptr;
    };
}
