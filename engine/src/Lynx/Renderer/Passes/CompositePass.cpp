#include "CompositePass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    struct CompositePushData
    {
        float BloomIntensity;
        int FXAAEnabled;
        float Padding[2];
    };
    
    void CompositePass::Init(RenderContext& ctx)
    {
        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Composite.glsl");

        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::Pixel)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(2))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(CompositePushData)))
            .setBindingOffsets({0, 0, 0, 0});
        m_BindingLayout = ctx.Device->createBindingLayout(layoutDesc);

        auto pipeDesc = nvrhi::GraphicsPipelineDesc()
            .addBindingLayout(m_BindingLayout)
            .setVertexShader(shader->GetVertexShader())
            .setPixelShader(shader->GetPixelShader())
            .setPrimType(nvrhi::PrimitiveType::TriangleList);
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        nvrhi::FramebufferInfo fbInfo;
        fbInfo.addColorFormat(nvrhi::Format::BGRA8_UNORM);

        m_Pipeline = ctx.Device->createGraphicsPipeline(pipeDesc, fbInfo);
    }

    void CompositePass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        if (!m_BindingSet || m_CachedInput != renderData.SceneColorInput || m_CachedBloom != renderData.BloomTexture)
        {
            m_CachedInput = renderData.SceneColorInput;
            m_CachedBloom = renderData.BloomTexture;

            nvrhi::TextureHandle bloom = renderData.BloomTexture ? renderData.BloomTexture : ctx.BlackTexture;
            
            auto bsDesc = nvrhi::BindingSetDesc()
                .addItem(nvrhi::BindingSetItem::Texture_SRV(0, renderData.SceneColorInput))
                .addItem(nvrhi::BindingSetItem::Sampler(1, ctx.GetSampler(SamplerSettings())))
                .addItem(nvrhi::BindingSetItem::Texture_SRV(2, bloom))
                .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(CompositePushData)));
            m_BindingSet = ctx.Device->createBindingSet(bsDesc, m_BindingLayout);
        }

        CompositePushData push;
        push.BloomIntensity = renderData.BloomIntensity;
        push.FXAAEnabled = renderData.FXAAEnabled ? 1 : 0;
        ctx.CommandList->setPushConstants(&push, sizeof(CompositePushData));

        auto state = nvrhi::GraphicsState()
            .setPipeline(m_Pipeline)
            .setFramebuffer(renderData.TargetFramebuffer)
            .addBindingSet(m_BindingSet);

        const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
        state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
        state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));

        ctx.CommandList->setGraphicsState(state);
        ctx.CommandList->draw(nvrhi::DrawArguments().setVertexCount(3));
    }
}
