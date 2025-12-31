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

    struct ShadowPushData
    {
        float AlphaCutoff;
        float Padding[3];
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

        auto globalDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0)) // UBO
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(ShadowPushData)))
            .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(10))
            .setBindingOffsets({0, 0, 0, 0});
        m_GlobalBindingLayout = ctx.Device->createBindingLayout(globalDesc);

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

        auto pipeDesc = nvrhi::GraphicsPipelineDesc();
        pipeDesc.bindingLayouts = { m_GlobalBindingLayout, m_MaterialBindingLayout };
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
        CreateGlobalBindingSet(ctx, renderData);
        
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

        state.bindings = { m_GlobalBindingSet };
        ctx.CommandList->setGraphicsState(state);

        for (const auto& batch : renderData.OpaqueDrawCalls)
        {
            if (batch.InstanceCount <= 0)
                continue;

            if (!(batch.Key.RenderFlags & RenderFlags::ShadowPass))
                continue;

            const auto& submesh = batch.Key.Mesh->GetSubmeshes()[batch.Key.SubmeshIndex];

            auto material = submesh.Material.get();
            bool isMasked = (material->Mode == AlphaMode::Mask);

            if (isMasked)
            {
                auto specificState = state;
                specificState.bindings = { m_GlobalBindingSet, GetMaskedBindingSet(ctx, renderData, material) };
                specificState.vertexBuffers = { nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0) };
                specificState.indexBuffer = nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT);
                ctx.CommandList->setGraphicsState(specificState);
            }
            else
            {
                // TODO: We could optimize this by sorting Opaque vs Masked.
                state.bindings = { m_GlobalBindingSet, m_OpaqueBindingSet };
                state.vertexBuffers = { nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0) };
                state.indexBuffer = nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT);
                ctx.CommandList->setGraphicsState(state);
            }

            ShadowPushData push;
            push.AlphaCutoff = isMasked ? material->AlphaCutoff : -1.0f;

            ctx.CommandList->setPushConstants(&push, sizeof(ShadowPushData));

            ctx.CommandList->drawIndexed(nvrhi::DrawArguments()
                .setVertexCount(submesh.IndexCount)
                .setInstanceCount(batch.InstanceCount)
                .setStartInstanceLocation(batch.FirstInstance));

            //renderData.DrawCalls++;
            //renderData.IndexCount += submesh.IndexCount * batch.InstanceCount;
        }

        ctx.CommandList->endMarker();
    }

    nvrhi::BindingSetHandle ShadowPass::GetMaskedBindingSet(RenderContext& ctx, RenderData& renderData, Material* material)
    {
        if (m_MaskedBindingSets.contains(material))
        {
            auto& entry = m_MaskedBindingSets[material];
            if (entry.Version == material->GetVersion())
                return entry.BindingSet;
        }

        nvrhi::TextureHandle albedo = ctx.WhiteTexture;
        SamplerSettings samplerSettings;

        if (material->AlbedoTexture)
        {
            if (auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(material->AlbedoTexture))
            {
                if (auto handle = texture->GetTextureHandle())
                {
                    albedo = handle;
                    samplerSettings = texture->GetSamplerSettings();
                }
            }
        }

        auto bsDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, albedo))
            .addItem(nvrhi::BindingSetItem::Sampler(1, ctx.GetSampler(samplerSettings)));

        auto set = ctx.Device->createBindingSet(bsDesc, m_MaterialBindingLayout);
        uint32_t version = material ? material->GetVersion() : 0;
        m_MaskedBindingSets[material] = { set, version };
        return set;
    }

    void ShadowPass::CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData)
    {
        if (m_GlobalBindingSet && m_CachedInstanceBuffer == renderData.InstanceBuffer)
            return;
        
        m_CachedInstanceBuffer = renderData.InstanceBuffer;
        auto desc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ShadowConstantBuffer))
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(ShadowPushData)))
            .addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(10, renderData.InstanceBuffer));

        m_GlobalBindingSet = ctx.Device->createBindingSet(desc, m_GlobalBindingLayout);
    }
}
