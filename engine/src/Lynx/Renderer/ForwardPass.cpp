#include "ForwardPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    void ForwardPass::Init(RenderContext& ctx)
    {
        // Global Layout (Set 0)
        auto globalLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(PushData)))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)) // Shadow Map
            .addItem(nvrhi::BindingLayoutItem::Sampler(2))
            .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(10))
            .setBindingOffsets({0, 0, 0, 0});
        m_GlobalBindingLayout = ctx.Device->createBindingLayout(globalLayoutDesc);

        // Material Layout (Set 1)
        auto matLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)) // Albedo
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)) // Normal
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)) // MR
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(3)) // Emissive
            .addItem(nvrhi::BindingLayoutItem::Sampler(4))
            .setBindingOffsets({0, 0, 0, 0});
        m_MaterialBindingLayout = ctx.Device->createBindingLayout(matLayoutDesc);

        std::string shaderPath = ctx.PresentationFramebufferInfo.colorFormats.size() > 1 ? "engine/resources/Shaders/StandardEditor.glsl" : "engine/resources/Shaders/Standard.glsl";
        auto shaderAsset = Engine::Get().GetAssetManager().GetAsset<Shader>(shaderPath);

        auto pipeDesc = nvrhi::GraphicsPipelineDesc()
            .addBindingLayout(m_GlobalBindingLayout)
            .addBindingLayout(m_MaterialBindingLayout)
            .setVertexShader(shaderAsset->GetVertexShader())
            .setFragmentShader(shaderAsset->GetPixelShader())
            .setPrimType(nvrhi::PrimitiveType::TriangleList);
        pipeDesc.renderState.rasterState.frontCounterClockwise = true;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Position))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("NORMAL")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Normal))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TANGENT")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Tangent))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, TexCoord))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Color))
                .setElementStride(sizeof(Vertex))
        };
        pipeDesc.inputLayout = ctx.Device->createInputLayout(attributes, 5, shaderAsset->GetVertexShader());

        // Opaque
        pipeDesc.renderState.depthStencilState.depthTestEnable = true;
        pipeDesc.renderState.depthStencilState.depthWriteEnable = true;
        pipeDesc.renderState.blendState.targets[0].setBlendEnable(false);

        if (ctx.PresentationFramebufferInfo.colorFormats.size() > 1)
            pipeDesc.renderState.blendState.targets[1].setBlendEnable(false).setColorWriteMask(nvrhi::ColorMask::All);

        m_PipelineOpaque = ctx.Device->createGraphicsPipeline(pipeDesc, ctx.PresentationFramebufferInfo);

        // Transparent
        pipeDesc.renderState.depthStencilState.depthWriteEnable = false;
        pipeDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)
            .setDestBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha);

        m_PipelineTransparent = ctx.Device->createGraphicsPipeline(pipeDesc, ctx.PresentationFramebufferInfo);
    }

    void ForwardPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        CreateGlobalBindingSet(ctx, renderData);
        
        DrawBatches(ctx, renderData, renderData.OpaqueDrawCalls, m_PipelineOpaque);
        DrawQueue(ctx, renderData, renderData.TransparentQueue, m_PipelineTransparent);
    }

    nvrhi::BindingSetHandle ForwardPass::GetMaterialBindingSet(RenderContext& ctx, Material* material)
    {
        if (m_MaterialBindingSetCache.contains(material))
            return m_MaterialBindingSetCache[material];

        nvrhi::TextureHandle albedo = ctx.WhiteTexture;
        nvrhi::TextureHandle normal = ctx.NormalTexture;
        nvrhi::TextureHandle mr = ctx.MetallicRoughnessTexture;
        nvrhi::TextureHandle emissive = ctx.BlackTexture;
        SamplerSettings samplerSettings;

        if (material)
        {
            auto& assetManager = Engine::Get().GetAssetManager();
            if (material->AlbedoTexture)
            {
                if (auto tex = assetManager.GetAsset<Texture>(material->AlbedoTexture))
                {
                    albedo = tex->GetTextureHandle();
                    samplerSettings = tex->GetSamplerSettings();
                }
            }
            if (material->NormalMap)
            {
                if (auto tex = assetManager.GetAsset<Texture>(material->NormalMap))
                    normal = tex->GetTextureHandle();
            }
            if (material->MetallicRoughnessTexture)
            {
                if (auto tex = assetManager.GetAsset<Texture>(material->MetallicRoughnessTexture))
                    mr = tex->GetTextureHandle();
            }
            if (material->EmissiveTexture)
            {
                if (auto tex = assetManager.GetAsset<Texture>(material->EmissiveTexture))
                    emissive = tex->GetTextureHandle();
            }
        }
        auto desc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, albedo))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, normal))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(2, mr))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(3, emissive))
            .addItem(nvrhi::BindingSetItem::Sampler(4, ctx.GetSampler(samplerSettings)));

        return m_MaterialBindingSetCache[material] = ctx.Device->createBindingSet(desc, m_MaterialBindingLayout);
    }

    void ForwardPass::CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData)
    {
        if (m_GlobalBindingSet && m_CachedInstanceBuffer == renderData.InstanceBuffer)
            return;

        m_CachedInstanceBuffer = renderData.InstanceBuffer;
        auto desc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, ctx.GlobalConstantBuffer))
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(PushData)))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, renderData.ShadowMap)) // Shadow Map
            .addItem(nvrhi::BindingSetItem::Sampler(2, renderData.ShadowSampler))
            .addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(10, renderData.InstanceBuffer));

        m_GlobalBindingSet = ctx.Device->createBindingSet(desc, m_GlobalBindingLayout);
    }

    void ForwardPass::DrawQueue(RenderContext& ctx, RenderData& renderData, std::vector<RenderCommand>& queue, nvrhi::GraphicsPipelineHandle pipeline)
    {
        if (queue.empty())
            return;

        for (const auto& cmd : queue)
        {
            if (!(cmd.Flags & RenderFlags::MainPass))
                continue;
            
            const auto& submesh = cmd.Mesh->GetSubmeshes()[cmd.SubmeshIndex];
            auto state = nvrhi::GraphicsState()
                .setPipeline(pipeline)
                .setFramebuffer(renderData.TargetFramebuffer)
                .addVertexBuffer(nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0))
                .setIndexBuffer(nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT));

            const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
            state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
            state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));
            state.addBindingSet(m_GlobalBindingSet);
            state.addBindingSet(GetMaterialBindingSet(ctx, submesh.Material.get()));

            ctx.CommandList->setGraphicsState(state);

            PushData push;
            push.AlphaCutoff = submesh.Material->Mode == AlphaMode::Mask ? submesh.Material->AlphaCutoff : -1.0f;
            ctx.CommandList->setPushConstants(&push, sizeof(PushData));
            ctx.CommandList->drawIndexed(nvrhi::DrawArguments()
                .setVertexCount(submesh.IndexCount)
                .setInstanceCount(1)
                .setStartInstanceLocation(cmd.InstanceOffset));

            renderData.DrawCalls++;
            renderData.IndexCount += submesh.IndexCount;
        }
    }

    void ForwardPass::DrawBatches(RenderContext& ctx, RenderData& renderData, std::vector<BatchDrawCall>& batches, nvrhi::GraphicsPipelineHandle pipeline)
    {
        for (const auto& batch : batches)
        {
            if (batch.InstanceCount <= 0)
                continue;

            if (!(batch.Key.RenderFlags & RenderFlags::MainPass))
                continue;

            const auto& submesh = batch.Key.Mesh->GetSubmeshes()[batch.Key.SubmeshIndex];
            auto state = nvrhi::GraphicsState()
                .setPipeline(pipeline)
                .setFramebuffer(renderData.TargetFramebuffer)
                .addVertexBuffer(nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0))
                .setIndexBuffer(nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT));

            const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
            state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
            state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));
            state.addBindingSet(m_GlobalBindingSet);
            state.addBindingSet(GetMaterialBindingSet(ctx, submesh.Material.get()));

            ctx.CommandList->setGraphicsState(state);

            PushData push;
            push.AlphaCutoff = submesh.Material->Mode == AlphaMode::Mask ? submesh.Material->AlphaCutoff : -1.0f;
            ctx.CommandList->setPushConstants(&push, sizeof(PushData));

            ctx.CommandList->drawIndexed(nvrhi::DrawArguments()
                .setVertexCount(submesh.IndexCount)
                .setInstanceCount(batch.InstanceCount)
                .setStartInstanceLocation(batch.FirstInstance));
            
            renderData.DrawCalls++;
            renderData.IndexCount += submesh.IndexCount * batch.InstanceCount;
        }
    }
}
