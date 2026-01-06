#pragma once

#include "Lynx/Core.h"
#include "Lynx/UI/Core/UICanvas.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace Lynx
{
    struct UIVertex
    {
        glm::vec3 Position;
        glm::vec2 UV;
        glm::vec4 Color;
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
        uint32_t GetIndexCount() const { return (uint32_t)m_Indices.size(); }
        bool HasData() const { return !m_Indices.empty(); }

    private:
        void TraverseAndCollect(std::shared_ptr<UIElement> element, const glm::vec2& parentAbsPos, float scale);
        void AddQuad(const UIRect& rect, const glm::vec4& color);
        void ResizeBuffers(size_t numVerts, size_t numIndices);

    private:
        nvrhi::DeviceHandle m_Device;

        std::vector<UIVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        nvrhi::BufferHandle m_VertexBuffer;
        nvrhi::BufferHandle m_IndexBuffer;
        size_t m_VertexBufferSize;
        size_t m_IndexBufferSize;
    };
}
