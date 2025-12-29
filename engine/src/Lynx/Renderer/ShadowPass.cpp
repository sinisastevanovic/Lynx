#include "ShadowPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "nvrhi/utils.h"

namespace Lynx
{
    struct ShadowSceneData
    {
        glm::mat4 ViewProjectionMatrix;
    };
    
    ShadowPass::ShadowPass(uint32_t resolution)
        : m_Resolution(resolution)
    {
    }

    void ShadowPass::Init(RenderContext& ctx)
    {
        auto shadowDesc = nvrhi::TextureDesc()
            .setWidth(m_Resolution).setHeight(m_Resolution)
            .setFormat(nvrhi::Format::D32)
            .setIsRenderTarget(true)
            .setDebugName("ShadowMap")
            .setInitialState(nvrhi::ResourceStates::DepthWrite)
            .setKeepInitialState(true);
        m_ShadowMap = ctx.Device->createTexture(shadowDesc);

        auto fbDesc = nvrhi::FramebufferDesc().setDepthAttachment(m_ShadowMap);
        m_Framebuffer = ctx.Device->createFramebuffer(fbDesc);

        auto cbDesc = nvrhi::BufferDesc()
            .setByteSize(sizeof(ShadowSceneData))
            .setIsConstantBuffer(true)
            .setDebugName("ShadowCB")
            .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
            .setKeepInitialState(true);
        m_ShadowConstantBuffer = ctx.Device->createBuffer(cbDesc);

        auto samplerDesc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Border)
            .setBorderColor(nvrhi::Color(1.0f))
            .setReductionType(nvrhi::SamplerReductionType::Comparison);
        m_ShadowSampler = ctx.Device->createSampler(samplerDesc);

        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Shadow.glsl");
        LX_ASSERT(shader, "Failed to load Shadow Shader!");

        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0)) // UBO
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1))    // Albedo (for alpha mask)
            .addItem(nvrhi::BindingLayoutItem::Sampler(2))        // Sampler
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(PushData)))
            .setBindingOffsets({0, 0, 0, 0});
        m_BindingLayout = ctx.Device->createBindingLayout(layoutDesc);

        auto bsDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ShadowConstantBuffer))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, ctx.WhiteTexture))
            .addItem(nvrhi::BindingSetItem::Sampler(2, ctx.GetSampler({})));
        m_DefaultBindingSet = ctx.Device->createBindingSet(bsDesc, m_BindingLayout);

        auto pipeDesc = nvrhi::GraphicsPipelineDesc();
        pipeDesc.bindingLayouts = { m_BindingLayout };
        pipeDesc.VS = shader->GetVertexShader();
        pipeDesc.PS = shader->GetPixelShader();
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;
        pipeDesc.renderState.rasterState.frontCounterClockwise = true;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;
        pipeDesc.renderState.rasterState.depthBias = 1.0f;
        pipeDesc.renderState.rasterState.slopeScaledDepthBias = 2.0f;
        pipeDesc.renderState.depthStencilState.depthTestEnable = true;
        pipeDesc.renderState.depthStencilState.depthWriteEnable = true;
        pipeDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Less;

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
                .setFormat(nvrhi::Format::RGBA32_FLOAT) // Note: RGBA for Tangent
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
        pipeDesc.inputLayout = ctx.Device->createInputLayout(attributes, 5, shader->GetVertexShader());

        m_Pipeline = ctx.Device->createGraphicsPipeline(pipeDesc, m_Framebuffer->getFramebufferInfo());
    }

    void ShadowPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        renderData.ShadowMap = m_ShadowMap;
        renderData.ShadowSampler = m_ShadowSampler;

        ctx.CommandList->beginMarker("ShadowPass");
        nvrhi::utils::ClearDepthStencilAttachment(ctx.CommandList, m_Framebuffer, 1.0f, 0);

        ShadowSceneData shadowData;
        shadowData.ViewProjectionMatrix = renderData.LightViewProj;
        ctx.CommandList->writeBuffer(m_ShadowConstantBuffer, &shadowData, sizeof(ShadowSceneData));

        auto state = nvrhi::GraphicsState()
            .setPipeline(m_Pipeline)
            .setFramebuffer(m_Framebuffer);
        state.viewport.addViewport(nvrhi::Viewport(m_Resolution, m_Resolution));
        state.viewport.addScissorRect(nvrhi::Rect(0, m_Resolution, 0, m_Resolution));

        state.bindings = { m_DefaultBindingSet };
        ctx.CommandList->setGraphicsState(state);

        for (const auto& cmd : renderData.OpaqueQueue)
        {
            const auto& submesh = cmd.Mesh->GetSubmeshes()[cmd.SubmeshIndex];

            auto material = submesh.Material.get();
            bool isMasked = (material->Mode == AlphaMode::Mask);

            if (isMasked)
            {
                auto specificState = state;
                specificState.bindings = { GetMaskedBindingSet(ctx, material) };
                specificState.vertexBuffers = { nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0) };
                specificState.indexBuffer = nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT);
                ctx.CommandList->setGraphicsState(specificState);
            }
            else
            {
                // TODO: We could optimize this by sorting Opaque vs Masked.
                state.vertexBuffers = { nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0) };
                state.indexBuffer = nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT);
                ctx.CommandList->setGraphicsState(state);
            }

            PushData push;
            push.Model = cmd.Transform;
            push.AlphaCutoff = isMasked ? material->AlphaCutoff : 1.0f;
            ctx.CommandList->setPushConstants(&push, sizeof(PushData));
            ctx.CommandList->drawIndexed(nvrhi::DrawArguments().setVertexCount(submesh.IndexCount));
        }

        ctx.CommandList->endMarker();
    }

    nvrhi::BindingSetHandle ShadowPass::GetMaskedBindingSet(RenderContext& ctx, Material* material)
    {
        if (m_MaskedBindingSets.contains(material))
            return m_MaskedBindingSets[material];

        nvrhi::TextureHandle albedo = ctx.WhiteTexture;
        SamplerSettings samplerSettings;

        if (material->AlbedoTexture)
        {
            if (auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(material->AlbedoTexture))
            {
                albedo = texture->GetTextureHandle();
                samplerSettings = texture->GetSamplerSettings();
            }
        }

        auto bsDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ShadowConstantBuffer))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, albedo))
            .addItem(nvrhi::BindingSetItem::Sampler(2, ctx.GetSampler(samplerSettings)));

        return m_MaskedBindingSets[material] = ctx.Device->createBindingSet(bsDesc, m_BindingLayout);
    }
}
