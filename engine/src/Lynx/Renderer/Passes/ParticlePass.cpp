#include "ParticlePass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/SamplerCache.h"

namespace Lynx
{
    struct ParticleVertex
    {
        glm::vec2 Pos;
        glm::vec2 UV;
    };

    struct ParticlePushData
    {
        glm::vec4 AlbedoColor;
        float EmissiveStrength;
        float Padding[3];
    };
    
    void ParticlePass::Init(RenderContext& ctx)
    {
        ParticleVertex vertices[] = {
            { {-0.5f, -0.5f}, {0.0f, 1.0f} },
            { { 0.5f, -0.5f}, {1.0f, 1.0f} },
            { { 0.5f,  0.5f}, {1.0f, 0.0f} },
            { {-0.5f,  0.5f}, {0.0f, 0.0f} }
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        auto vbDesc = nvrhi::BufferDesc()
            .setByteSize(sizeof(vertices))
            .setIsVertexBuffer(true)
            .setDebugName("ParticleQuadVB")
            .setInitialState(nvrhi::ResourceStates::VertexBuffer)
            .setKeepInitialState(true);
        m_QuadVertexBuffer = ctx.Device->createBuffer(vbDesc);

        auto ibDesc = nvrhi::BufferDesc()
            .setByteSize(sizeof(indices))
            .setIsIndexBuffer(true)
            .setDebugName("ParticleQuadIB")
            .setInitialState(nvrhi::ResourceStates::IndexBuffer)
            .setKeepInitialState(true);
        m_QuadIndexBuffer = ctx.Device->createBuffer(ibDesc);

        auto cmd = ctx.Device->createCommandList();
        cmd->open();
        cmd->writeBuffer(m_QuadVertexBuffer, vertices, sizeof(vertices));
        cmd->writeBuffer(m_QuadIndexBuffer, indices, sizeof(indices));
        cmd->close();
        ctx.Device->executeCommandList(cmd);

        auto globalLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
            .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(ParticlePushData)))
            .setBindingOffsets({ 0, 0, 0, 0 });
        m_GlobalBindingLayout = ctx.Device->createBindingLayout(globalLayoutDesc);

        auto materialLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::Pixel)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))
            .setBindingOffsets({ 0, 0, 0, 0 });
        m_MaterialBindingLayout = ctx.Device->createBindingLayout(materialLayoutDesc);

        m_PipelineState.SetPath("engine/resources/Shaders/Particle.glsl");
        m_PipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            CreatePipelines(ctx, shader);
        });
    }

    void ParticlePass::CreatePipelines(RenderContext& ctx, std::shared_ptr<Shader> shader)
    {
        auto pipeDesc = nvrhi::GraphicsPipelineDesc()
            .addBindingLayout(m_GlobalBindingLayout)
            .addBindingLayout(m_MaterialBindingLayout)
            .setVertexShader(shader->GetVertexShader())
            .setPixelShader(shader->GetPixelShader())
            .setPrimType(nvrhi::PrimitiveType::TriangleList);

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POS")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(0)
                .setElementStride(sizeof(ParticleVertex)),
            nvrhi::VertexAttributeDesc()
                .setName("UV")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(sizeof(glm::vec2))
                .setElementStride(sizeof(ParticleVertex))
        };
        pipeDesc.inputLayout = ctx.Device->createInputLayout(attributes, 2, shader->GetVertexShader());

        // Common State
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None; // Particles represent 2D sprites, usually seen from any side, but mostly front.
        pipeDesc.renderState.depthStencilState.depthWriteEnable = false; // Soft particles
        pipeDesc.renderState.depthStencilState.depthTestEnable = true;

        // Alpha Blend Pipeline
        pipeDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha);
        m_PipelineAlpha = ctx.Device->createGraphicsPipeline(pipeDesc, ctx.PresentationFramebufferInfo);

        // Additive Blend Pipeline
        pipeDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::One);
        m_PipelineAdditive = ctx.Device->createGraphicsPipeline(pipeDesc, ctx.PresentationFramebufferInfo);
    }

    void ParticlePass::CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData)
    {
        if (m_GlobalBindingSet && m_CachedInstanceBuffer == renderData.ParticleInstanceBuffer)
            return;

        m_CachedInstanceBuffer = renderData.ParticleInstanceBuffer;
        auto desc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, ctx.GlobalConstantBuffer))
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(ParticlePushData)))
            .addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(1, renderData.ParticleInstanceBuffer));

        m_GlobalBindingSet = ctx.Device->createBindingSet(desc, m_GlobalBindingLayout);
    }

    void ParticlePass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        if (renderData.ParticleQueue.empty())
            return;

        m_PipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            CreatePipelines(ctx, shader);
        });

        CreateGlobalBindingSet(ctx, renderData);

        for (const auto& batch : renderData.ParticleQueue)
        {
            auto pipeline = (batch.Material->Mode == AlphaMode::Additive) ? m_PipelineAdditive : m_PipelineAlpha;

            auto state = nvrhi::GraphicsState()
                .setPipeline(pipeline)
                .setFramebuffer(renderData.TargetFramebuffer)
                .addVertexBuffer(nvrhi::VertexBufferBinding(m_QuadVertexBuffer, 0, 0))
                .setIndexBuffer(nvrhi::IndexBufferBinding(m_QuadIndexBuffer, nvrhi::Format::R32_UINT))
                .addBindingSet(m_GlobalBindingSet)
                .addBindingSet(GetMaterialBindingSet(ctx, batch.Material));

            const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
            state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
            state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));

            ctx.CommandList->setGraphicsState(state);

            ParticlePushData push;
            push.AlbedoColor = batch.Material->AlbedoColor;
            push.EmissiveStrength = batch.Material->EmissiveStrength;
            ctx.CommandList->setPushConstants(&push, sizeof(ParticlePushData));
            
            ctx.CommandList->drawIndexed(nvrhi::DrawArguments()
                .setVertexCount(6)
                .setInstanceCount(batch.Count)
                .setStartInstanceLocation(batch.StartOffset));

            renderData.DrawCalls++;
        }
    }

    nvrhi::BindingSetHandle ParticlePass::GetMaterialBindingSet(RenderContext& ctx, Material* material)
    {
        if (material && m_MaterialBindingSetCache.contains(material))
        {
            auto& entry = m_MaterialBindingSetCache[material];
            if (entry.Version == material->GetVersion())
                return entry.BindingSet;
        }

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

        auto newSet = ctx.Device->createBindingSet(desc, m_MaterialBindingLayout);
        uint32_t currentVersion = material ? material->GetVersion() : 0;
        m_MaterialBindingSetCache[material] = { newSet, currentVersion };
        
        return newSet;
    }
}
