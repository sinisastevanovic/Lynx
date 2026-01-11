#pragma once

#include "Lynx/Core.h"
#include "Lynx/UI/Core/UICanvas.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "Lynx/Asset/Font.h"

namespace Lynx
{
    class Material;
    class Texture;
    
    struct UIVertex
    {
        glm::vec3 Position;
        glm::vec2 UV;
        uint32_t Color;
        uint32_t OutlineColor = 0;
        float OutlineWidth = 0.0f;
        glm::vec4 ClipRect;
    };

    enum class UIBatchType { Standard, Text };

    struct UIBatch
    {
        UIBatchType Type = UIBatchType::Standard;
        std::shared_ptr<Material> Material;
        std::shared_ptr<Texture> Texture;
        float PixelRange = 0.0f;

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

        void DrawRect(const UIRect& rect, const glm::vec4& color, std::shared_ptr<Material> material, std::shared_ptr<Texture> textureOverride,
            glm::vec2 uvMin = {0.0f, 0.0f}, glm::vec2 uvMax = {1.0f, 1.0f});
        void DrawNineSlice(const UIRect& rect, const UIThickness& border, const glm::vec4& color, std::shared_ptr<Material> material, std::shared_ptr<Texture> textureOverride,
            glm::vec2 uvMin = {0.0f, 0.0f}, glm::vec2 uvMax = {1.0f, 1.0f});
        void DrawString(const std::string& text, std::shared_ptr<Font> font, float fontSize, glm::vec2 position, const glm::vec4& color, const glm::vec4& outlineColor, float outlineWidth);

    private:
        void TraverseAndCollect(std::shared_ptr<UIElement> element, float scale, float parentOpacity, glm::vec4 parentTint);
        void AddQuad(const UIRect& rect, const glm::vec4& color, glm::vec2 uvMin, glm::vec2 uvMax, const glm::vec4& outlineColor = glm::vec4(0), float outlineWidth = 0.0f);
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
        std::shared_ptr<Texture> m_CurrentTexture = nullptr;
        float m_CurrentOpacity = 1.0f;
        
        std::vector<glm::vec4> m_ClipStack;
        glm::vec4 m_CurrentClipRect;
    };
}
