#include "UIBatcher.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/Texture.h"

namespace Lynx
{
    UIBatcher::UIBatcher(nvrhi::DeviceHandle device)
        : m_Device(device)
    {
        ResizeBuffers(4096, 6144); // enough for ~1000 quads
    }
    
    void UIBatcher::ResizeBuffers(size_t numVerts, size_t numIndices)
    {
        if (numVerts > m_VertexBufferSize || !m_VertexBuffer)
        {
            m_VertexBufferSize = std::max(numVerts, (size_t)4096);

            auto desc = nvrhi::BufferDesc()
                .setByteSize(m_VertexBufferSize * sizeof(UIVertex))
                .setIsVertexBuffer(true)
                .setDebugName("UI_VertexBuffer")
                .setInitialState(nvrhi::ResourceStates::VertexBuffer)
                .setKeepInitialState(true);
            m_VertexBuffer = m_Device->createBuffer(desc);
        }

        if (numIndices > m_IndexBufferSize || !m_IndexBuffer)
        {
            m_IndexBufferSize = std::max(numVerts, (size_t)6144);

            auto desc = nvrhi::BufferDesc()
                .setByteSize(m_IndexBufferSize * sizeof(uint32_t))
                .setIsIndexBuffer(true)
                .setDebugName("UI_IndexBuffer")
                .setInitialState(nvrhi::ResourceStates::IndexBuffer)
                .setKeepInitialState(true);
            m_IndexBuffer = m_Device->createBuffer(desc);
        }
    }

    void UIBatcher::Begin()
    {
        m_Vertices.clear();
        m_Indices.clear();
        m_Batches.clear();
        m_CurrentTexture = nullptr;
        m_CurrentMaterial = nullptr;
    }

    void UIBatcher::Submit(std::shared_ptr<UICanvas> canvas)
    {
        if (!canvas)
            return;

        float scale = canvas->GetScaleFactor();
        TraverseAndCollect(canvas, scale, canvas->GetOpacity());
    }

    void UIBatcher::DrawRect(const UIRect& rect, const glm::vec4& color, std::shared_ptr<Material> material, std::shared_ptr<Texture> textureOverride,
        glm::vec2 uvMin, glm::vec2 uvMax)
    {
        std::shared_ptr<Texture> resolvedTexture = nullptr;
        if (textureOverride && textureOverride->GetTextureHandle())
        {
            resolvedTexture = textureOverride;
        }
        else if (material && material->AlbedoTexture)
        {
            auto texAsset = Engine::Get().GetAssetManager().GetAsset<Texture>(material->AlbedoTexture);
            if (texAsset && texAsset->GetTextureHandle())
                resolvedTexture = texAsset;
        }

        if (!resolvedTexture)
        {
            resolvedTexture = Engine::Get().GetAssetManager().GetWhiteTexture();
        }

        if (resolvedTexture != m_CurrentTexture || material != m_CurrentMaterial || m_Batches.empty())
        {
            m_CurrentTexture = resolvedTexture;
            m_CurrentMaterial = material;

            UIBatch newBatch;
            newBatch.Material = material;
            newBatch.Texture = resolvedTexture;
            newBatch.IndexStart = (uint32_t)m_Indices.size();
            newBatch.IndexCount = 0;
            m_Batches.push_back(newBatch);
        }

        AddQuad(rect, color, uvMin, uvMax);
        m_Batches.back().IndexCount += 6;
    }

    void UIBatcher::DrawNineSlice(const UIRect& rect, const UIThickness& border, const glm::vec4& color, std::shared_ptr<Material> material,
        std::shared_ptr<Texture> textureOverride, glm::vec2 uvMin, glm::vec2 uvMax)
    {
        std::shared_ptr<Texture> resolvedTexture = nullptr;
        if (textureOverride && textureOverride->GetTextureHandle())
        {
            resolvedTexture = textureOverride;
        }
        else if (material && material->AlbedoTexture)
        {
            auto texAsset = Engine::Get().GetAssetManager().GetAsset<Texture>(material->AlbedoTexture);
            if (texAsset && texAsset->GetTextureHandle())
                resolvedTexture = texAsset;
        }

        if (!resolvedTexture)
        {
            resolvedTexture = Engine::Get().GetAssetManager().GetWhiteTexture();
        }

        if (resolvedTexture != m_CurrentTexture || material != m_CurrentMaterial || m_Batches.empty())
        {
            m_CurrentTexture = resolvedTexture;
            m_CurrentMaterial = material;

            UIBatch newBatch;
            newBatch.Material = material;
            newBatch.Texture = resolvedTexture;
            newBatch.IndexStart = (uint32_t)m_Indices.size();
            newBatch.IndexCount = 0;
            m_Batches.push_back(newBatch);
        }

        // TODO:
        // BETTER: The user provides border in PIXELS (or DP). We convert to UVs.
        
        float x[4] = { rect.X, rect.X + border.Left, rect.X + rect.Width - border.Right, rect.X + rect.Width };
        float y[4] = { rect.Y, rect.Y + border.Top, rect.Y + rect.Height - border.Bottom, rect.Y + rect.Height };

        uint32_t texW = 1, texH = 1;
        if (resolvedTexture) {
            texW = resolvedTexture->GetWidth();
            texH = resolvedTexture->GetHeight();
        }

        float borderU_Left   = border.Left / texW;
        float borderU_Right  = border.Right / texW;
        float borderV_Top    = border.Top / texH;
        float borderV_Bottom = border.Bottom / texH;

        float u[4] = {
            uvMin.x,
            uvMin.x + borderU_Left,
            uvMax.x - borderU_Right,
            uvMax.x
        };

        float v[4] = {
            uvMin.y,
            uvMin.y + borderV_Top,
            uvMax.y - borderV_Bottom,
            uvMax.y
        };

        uint32_t startIndex = (uint32_t)m_Vertices.size();
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                m_Vertices.push_back({ { x[i], y[j], 0.0f }, { u[i], v[j] }, color });
            }
        }

        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                // Row j, Col i
                uint32_t i0 = startIndex + (j * 4) + i;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + 4;
                uint32_t i3 = i2 + 1;

                m_Indices.push_back(i0); m_Indices.push_back(i2); m_Indices.push_back(i1);
                m_Indices.push_back(i1); m_Indices.push_back(i2); m_Indices.push_back(i3);

                m_Batches.back().IndexCount += 6;
            }
        }
    }

    void UIBatcher::TraverseAndCollect(std::shared_ptr<UIElement> element, float scale, float parentOpacity)
    {
        // TODO: Should we only upload if ui is dirty??
        if (element->GetVisibility() != UIVisibility::Visible)
            return;

        float finalOpacity = parentOpacity * element->GetOpacity();
        m_CurrentOpacity = finalOpacity;
        if (finalOpacity <= 0.01f)
            return;

        UIRect cachedRect = element->GetCachedRect();

        UIRect pixelRect;
        pixelRect.X = cachedRect.X * scale;
        pixelRect.Y = cachedRect.Y * scale;
        pixelRect.Width = cachedRect.Width * scale;
        pixelRect.Height = cachedRect.Height * scale;

        // DEBUG VISUALIZATION
        /*if (element->GetName() == "ButtonStack")
        {
            size_t hash = (size_t)element.get();
            float r = ((hash & 0xFF0000) >> 16) / 255.0f;
            float g = ((hash & 0x00FF00) >> 8) / 255.0f;
            float b = (hash & 0x0000FF) / 255.0f;

            AddQuad(pixelRect, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), {0.0f, 0.0f}, {1.0f, 1.0f}); // 50% opacity
        }*/

        element->OnDraw(*this, pixelRect);

        for (const auto& child : element->GetChildren())
        {
            TraverseAndCollect(child, scale, finalOpacity);
        }
    }
    
    void UIBatcher::AddQuad(const UIRect& rect, const glm::vec4& color, glm::vec2 uvMin, glm::vec2 uvMax)
    {
        uint32_t startIndex = (uint32_t)m_Vertices.size();

        glm::vec4 finalColor = color;
        finalColor.a *= m_CurrentOpacity;

        // 0 -- 1
        // |    |
        // 2 -- 3

        // Top Left
        m_Vertices.push_back({
            { rect.X, rect.Y, 0.0f },
            { uvMin.x, uvMin.y },
            finalColor
        });

        // Top Right
        m_Vertices.push_back({
            { rect.X + rect.Width, rect.Y, 0.0f },
            { uvMax.x, uvMin.y  },
            finalColor
        });

        // Bottom Left
        m_Vertices.push_back({
            { rect.X, rect.Y + rect.Height, 0.0f },
            { uvMin.x, uvMax.y },
            finalColor
        });

        // Bottom Right
        m_Vertices.push_back({
            { rect.X + rect.Width, rect.Y + rect.Height, 0.0f },
            { uvMax.x, uvMax.y },
            finalColor
        });

        // CCW winding (0-2-1, 1-2-3) or similar depending on cull mode.
        // Standard NVRHI/Vulkan usually wants Clockwise or check CullMode.
        // Let's use: 0->1->2, 2->1->3

        m_Indices.push_back(startIndex + 0);
        m_Indices.push_back(startIndex + 2);
        m_Indices.push_back(startIndex + 1);

        m_Indices.push_back(startIndex + 1);
        m_Indices.push_back(startIndex + 2);
        m_Indices.push_back(startIndex + 3);
    }
    
    void UIBatcher::Upload(nvrhi::CommandListHandle commandList)
    {
        if (m_Vertices.empty())
            return;

        ResizeBuffers(m_Vertices.size(), m_Indices.size());

        commandList->writeBuffer(m_VertexBuffer, m_Vertices.data(), m_Vertices.size() * sizeof(UIVertex));
        commandList->writeBuffer(m_IndexBuffer, m_Indices.data(), m_Indices.size() * sizeof(uint32_t));
    }
}
