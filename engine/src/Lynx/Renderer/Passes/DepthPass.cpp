#include "DepthPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    void DepthPass::Init(RenderContext& ctx)
    {
        auto globalLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(PushData)))
            .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(10))
            .setBindingOffsets({0, 0, 0, 0});
        m_GlobalBindingLayout = ctx.Device->createBindingLayout(globalLayoutDesc);

        auto matDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))    // Albedo (for alpha mask)
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))        // Sampler
            .setBindingOffsets({0, 0, 0, 0});
        m_MaterialBindingLayout = ctx.Device->createBindingLayout(matDesc);

        auto bsDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, ctx.WhiteTexture))
            .addItem(nvrhi::BindingSetItem::Sampler(1, ctx.GetSampler(SamplerSettings())));
        m_OpaqueBindingSet = ctx.Device->createBindingSet(bsDesc, m_MaterialBindingLayout);

        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/DepthOnly.glsl");

        auto pipeDesc = nvrhi::GraphicsPipelineDesc()
            .addBindingLayout(m_GlobalBindingLayout)
            .addBindingLayout(m_MaterialBindingLayout)
            .setVertexShader(shader->GetVertexShader())
            .setFragmentShader(shader->GetPixelShader()) // Needed for Alpha Test
            .setPrimType(nvrhi::PrimitiveType::TriangleList);

        pipeDesc.renderState.rasterState.frontCounterClockwise = true;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;

        // Depth State: Write ON, Test Less
        pipeDesc.renderState.depthStencilState.depthTestEnable = true;
        pipeDesc.renderState.depthStencilState.depthWriteEnable = true;
        pipeDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Less;

        // Color State: Disable Writes
        pipeDesc.renderState.blendState.targets[0].setBlendEnable(false)
                                                  .setColorWriteMask(static_cast<nvrhi::ColorMask>(0)); // TODO: is this correct?

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Position))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, TexCoord))
                .setElementStride(sizeof(Vertex))
        };
        pipeDesc.inputLayout = ctx.Device->createInputLayout(attributes, 2, shader->GetVertexShader());
        m_Pipeline = ctx.Device->createGraphicsPipeline(pipeDesc, ctx.PresentationFramebufferInfo);
    }

    void DepthPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        CreateGlobalBindingSet(ctx, renderData);

        ctx.CommandList->beginMarker("DepthPrePass");

        auto state = nvrhi::GraphicsState()
            .setPipeline(m_Pipeline)
            .setFramebuffer(renderData.TargetFramebuffer); // Main FB

        // Viewport
        const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
        state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
        state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));

        state.bindings = { m_GlobalBindingSet };
        ctx.CommandList->setGraphicsState(state);

        for (const auto& batch : renderData.OpaqueDrawCalls)
        {
            if (!(batch.Key.RenderFlags & RenderFlags::MainPass))
                continue;

            const auto& submesh = batch.Key.Mesh->GetSubmeshes()[batch.Key.SubmeshIndex];
            auto material = submesh.Material.get();
            bool isMasked = (material->Mode == AlphaMode::Mask);

            state.bindings = { m_GlobalBindingSet, isMasked ? GetMaterialBindingSet(ctx, material) : m_OpaqueBindingSet };

            state.vertexBuffers = { nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0) };
            state.indexBuffer = nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT);
            ctx.CommandList->setGraphicsState(state);

            PushData push;
            push.AlphaCutoff = isMasked ? material->AlphaCutoff : -1.0f;
            ctx.CommandList->setPushConstants(&push, sizeof(PushData));

            ctx.CommandList->drawIndexed(nvrhi::DrawArguments()
                .setVertexCount(submesh.IndexCount)
                .setInstanceCount(batch.InstanceCount)
                .setStartInstanceLocation(batch.FirstInstance));

            // Note: Don't double count stats here!
            // Or count them as "DepthDrawCalls" if you want separate metrics.
        }

        ctx.CommandList->endMarker();
    }

    void DepthPass::CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData)
    {
        if (m_GlobalBindingSet && m_CachedInstanceBuffer == renderData.InstanceBuffer)
            return;

        m_CachedInstanceBuffer = renderData.InstanceBuffer;
        auto desc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, ctx.GlobalConstantBuffer))
            .addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(10, renderData.InstanceBuffer));

        m_GlobalBindingSet = ctx.Device->createBindingSet(desc, m_GlobalBindingLayout);
    }

    nvrhi::BindingSetHandle DepthPass::GetMaterialBindingSet(RenderContext& ctx, Material* material)
    {
        if (m_MaterialBindingSetCache.contains(material))
            return m_MaterialBindingSetCache[material];

        nvrhi::TextureHandle albedo = ctx.WhiteTexture;
        SamplerSettings samplerSettings;

        if (material->AlbedoTexture)
        {
            if (auto tex = Engine::Get().GetAssetManager().GetAsset<Texture>(material->AlbedoTexture))
            {
                albedo = tex->GetTextureHandle();
                samplerSettings = tex->GetSamplerSettings();
            }
        }

        auto desc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, albedo))
            .addItem(nvrhi::BindingSetItem::Sampler(1, ctx.GetSampler(samplerSettings)));

        return m_MaterialBindingSetCache[material] = ctx.Device->createBindingSet(desc, m_MaterialBindingLayout);
    }
}
