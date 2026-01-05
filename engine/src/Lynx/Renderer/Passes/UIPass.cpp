#include "UIPass.h"

#include "Lynx/Asset/Shader.h"
#include <glm/ext/matrix_clip_space.hpp>

#include "Lynx/Engine.h"
#include "Lynx/Renderer/SamplerCache.h"

namespace Lynx
{
    struct UIVertex
    {
        glm::vec2 Pos;
        glm::vec2 UV;
    };
    
    struct UIConstants
    {
        glm::mat4 Projection;
    };
    
    void UIPass::Init(RenderContext& ctx)
    {
        UIVertex vertices[] = {
            { {0.0f, 0.0f}, {0.0f, 0.0f} }, // Bottom-Left
            { {1.0f, 0.0f}, {1.0f, 0.0f} }, // Bottom-Right
            { {1.0f, 1.0f}, {1.0f, 1.0f} }, // Top-Right
            { {0.0f, 1.0f}, {0.0f, 1.0f} }  // Top-Left
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        m_QuadVertexBuffer = ctx.Device->createBuffer(nvrhi::BufferDesc()
            .setByteSize(sizeof(vertices))
            .setIsVertexBuffer(true)
            .setDebugName("UIQuadVB")
            .setInitialState(nvrhi::ResourceStates::VertexBuffer)
            .setKeepInitialState(true));

        m_QuadIndexBuffer = ctx.Device->createBuffer(nvrhi::BufferDesc()
            .setByteSize(sizeof(indices))
            .setIsIndexBuffer(true)
            .setDebugName("UIQuadIB")
            .setInitialState(nvrhi::ResourceStates::IndexBuffer)
            .setKeepInitialState(true));

        auto cmdList = ctx.Device->createCommandList();
        cmdList->open();
        cmdList->writeBuffer(m_QuadVertexBuffer, vertices, sizeof(vertices));
        cmdList->writeBuffer(m_QuadIndexBuffer, indices, sizeof(indices));
        cmdList->close();
        ctx.Device->executeCommandList(cmdList);

        auto globalLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(UIConstants)))
            .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0))
            .setBindingOffsets({0, 0, 0, 0});
        m_GlobalBindingLayout = ctx.Device->createBindingLayout(globalLayoutDesc);

        auto matLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::Pixel)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))
            .setBindingOffsets({0, 0, 0, 0});
        m_MaterialBindingLayout = ctx.Device->createBindingLayout(matLayoutDesc);

        m_PipelineState.SetPath("engine/resources/Shaders/UI.glsl");
        m_PipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            this->CreatePipeline(ctx, shader);
        });
    }

    void UIPass::CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader)
    {
        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
            .setVertexShader(shader->GetVertexShader())
            .setPixelShader(shader->GetPixelShader())
            .addBindingLayout(m_GlobalBindingLayout)
            .addBindingLayout(m_MaterialBindingLayout)
            .setPrimType(nvrhi::PrimitiveType::TriangleList);
        
        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POS")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(0)
                .setElementStride(sizeof(UIVertex)),
            nvrhi::VertexAttributeDesc()
                .setName("UV")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(sizeof(glm::vec2))
                .setElementStride(sizeof(UIVertex))
        };
        
        // TODO: Create input layout
        pipelineDesc.setInputLayout(ctx.Device->createInputLayout(attributes, 2, shader->GetVertexShader()));
        pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
        pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
        pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipelineDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)      // Source Alpha
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)  // 1 - Source Alpha
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)
            .setDestBlendAlpha(nvrhi::BlendFactor::OneMinusSrcAlpha); // Or Zero/One

        m_Pipeline = ctx.Device->createGraphicsPipeline(pipelineDesc, ctx.FinalOutputFramebufferInfo);
    }

    nvrhi::BindingSetHandle UIPass::GetMaterialBindingSet(RenderContext& ctx, Material* material)
    {
        uint32_t version = material ? material->GetVersion() : 0;

        return m_MaterialBindingSetCache.Get(material, version, [&]() -> nvrhi::BindingSetHandle
        {
            nvrhi::TextureHandle albedo = ctx.WhiteTexture;
            SamplerSettings samplerSettings;

            if (material)
            {
                auto& assetManager = Engine::Get().GetAssetManager();
                if (material->AlbedoTexture)
                {
                    if (auto tex = assetManager.GetAsset<Texture>(material->AlbedoTexture))
                    {
                        if (auto handle = tex->GetTextureHandle())
                        {
                            albedo = handle;
                            samplerSettings = tex->GetSamplerSettings();
                        }
                    }
                }
            }
            auto desc = nvrhi::BindingSetDesc()
                .addItem(nvrhi::BindingSetItem::Texture_SRV(0, albedo))
                .addItem(nvrhi::BindingSetItem::Sampler(1, SamplerCache::Get()->GetSampler(samplerSettings)));

            return ctx.Device->createBindingSet(desc, m_MaterialBindingLayout);
        });
    }

    void UIPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        if (renderData.UIQueue.empty())
            return;
        
        m_PipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            this->CreatePipeline(ctx, shader);
        });

        CreateGlobalBindingSet(ctx, renderData);

        for (const auto& batch : renderData.UIQueue)
        {
            auto state = nvrhi::GraphicsState()
                .setPipeline(m_Pipeline)
                .setFramebuffer(renderData.TargetFramebuffer)
                .addVertexBuffer( {m_QuadVertexBuffer, 0, 0} )
                .setIndexBuffer( {m_QuadIndexBuffer, nvrhi::Format::R32_UINT, 0} )
                .addBindingSet(m_GlobalBindingSet)
                .addBindingSet(GetMaterialBindingSet(ctx, batch.Material));

            const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
            state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
            state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));
            ctx.CommandList->setGraphicsState(state);

            UIConstants push;
            push.Projection = glm::ortho(0.0f, (float)fbInfo.width, 0.0f, (float)fbInfo.height);
            ctx.CommandList->setPushConstants(&push, sizeof(UIConstants));

            ctx.CommandList->drawIndexed({ 6, batch.Count, 0, 0, batch.StartOffset });
            renderData.DrawCalls++;
        }
    }

    void UIPass::CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData)
    {
        if (m_GlobalBindingSet && m_CachedInstanceBuffer == renderData.UIInstanceBuffer)
            return;

        m_CachedInstanceBuffer = renderData.UIInstanceBuffer;
        auto desc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(UIConstants)))
            .addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, renderData.UIInstanceBuffer));
        m_GlobalBindingSet = ctx.Device->createBindingSet(desc, m_GlobalBindingLayout);
    }
}
