#include "BloomPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "nvrhi/utils.h"

namespace Lynx
{
    BloomPass::BloomPass()
    {
    }

    void BloomPass::Init(RenderContext& ctx)
    {
        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Bloom.glsl");

        m_BindingLayout = ctx.Device->createBindingLayout(nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::Pixel)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(PushData)))
            .setBindingOffsets({0, 0, 0, 0})
        );

        m_Sampler = SamplerCache::Get()->GetSampler(SamplerSettings{TextureWrap::Clamp, TextureFilter::Bilinear});

        m_PipelineState.SetPath("engine/resources/Shaders/Bloom.glsl");
        m_PipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            this->CreatePipeline(ctx, shader);
        });
    }
    
    void BloomPass::CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader)
    {
        auto pipeDesc = nvrhi::GraphicsPipelineDesc()
            .addBindingLayout(m_BindingLayout)
            .setVertexShader(shader->GetVertexShader())
            .setPixelShader(shader->GetPixelShader())
            .setPrimType(nvrhi::PrimitiveType::TriangleList);
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipeDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::One)
            .setDestBlend(nvrhi::BlendFactor::One)
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)
            .setDestBlendAlpha(nvrhi::BlendFactor::One);

        nvrhi::FramebufferInfo fbInfo;
        fbInfo.addColorFormat(nvrhi::Format::RGBA16_FLOAT);
        m_Pipeline = ctx.Device->createGraphicsPipeline(pipeDesc, fbInfo);
    }

    void BloomPass::EnsureResources(RenderContext& ctx, uint32_t width, uint32_t height)
    {
        uint32_t w = width / 2;
        uint32_t h = height / 2;

        if (m_BloomTexture && m_Width == w && m_Height == h)
            return;

        m_Width = w;
        m_Height = h;

        m_MipCount = std::min(MAX_MIPS, (uint32_t)std::floor(std::log2(std::max(w, h))) + 1);

        auto desc = nvrhi::TextureDesc()
            .setWidth(w)
            .setHeight(h)
            .setFormat(nvrhi::Format::RGBA16_FLOAT)
            .setMipLevels(m_MipCount)
            .setIsRenderTarget(true)
            .setDebugName("BloomChain")
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true);
        m_BloomTexture = ctx.Device->createTexture(desc);

        m_Framebuffers.clear();
        m_BindingSets.clear();

        for (uint32_t i = 0; i < m_MipCount; i++)
        {
            auto fbDesc = nvrhi::FramebufferDesc().addColorAttachment(
                nvrhi::FramebufferAttachment(m_BloomTexture).setSubresources(nvrhi::TextureSubresourceSet(i, 1, 0, 1))
            );
            m_Framebuffers.push_back(ctx.Device->createFramebuffer(fbDesc));

            auto bsDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_BloomTexture).setSubresources(nvrhi::TextureSubresourceSet(i, 1, 0, 1)))
            .addItem(nvrhi::BindingSetItem::Sampler(1, m_Sampler))
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(PushData)));

            m_BindingSets.push_back(ctx.Device->createBindingSet(bsDesc, m_BindingLayout));
        }
    }


    void BloomPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        if (!m_Settings.Enabled)
        {
            renderData.BloomIntensity = 0.0f;
            return;
        }

        m_PipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            this->CreatePipeline(ctx, shader);
        });
        
        // 1. Resize if needed
        const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
        EnsureResources(ctx, fbInfo.width, fbInfo.height);

        // 2. PREFILTER: Read Scene -> Write Mip 0
        if (!m_InputBindingSet || m_CachedInput != renderData.SceneColorInput)
        {
            m_CachedInput = renderData.SceneColorInput;
            m_InputBindingSet = ctx.Device->createBindingSet(nvrhi::BindingSetDesc()
                .addItem(nvrhi::BindingSetItem::Texture_SRV(0, renderData.SceneColorInput))
                .addItem(nvrhi::BindingSetItem::Sampler(1, m_Sampler))
                .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(PushData))), m_BindingLayout);
        }

        BloomPushData push;
        push.Params = glm::vec4(m_Settings.Threshold, m_Settings.Knee, m_Settings.Radius, 0.0f);
        push.Mode = 0; // Prefilter

        renderData.BloomIntensity = m_Settings.Intensity;

        auto state = nvrhi::GraphicsState()
            .setPipeline(m_Pipeline)
            .setFramebuffer(m_Framebuffers[0])
            .addBindingSet(m_InputBindingSet)
            .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(m_Width, m_Height)));

        ctx.CommandList->beginMarker("Bloom");

        nvrhi::utils::ClearColorAttachment(ctx.CommandList, m_Framebuffers[0], 0, nvrhi::Color(0.0f));
        ctx.CommandList->setGraphicsState(state);
        ctx.CommandList->setPushConstants(&push, sizeof(push));
        ctx.CommandList->draw(nvrhi::DrawArguments().setVertexCount(3));

        // 3. DOWNSAMPLE CHAIN
        push.Mode = 1;
        for (uint32_t i = 0; i < m_MipCount - 1; i++)
        {
            uint32_t dstWidth = std::max(1u, m_Width >> (i + 1));
            uint32_t dstHeight = std::max(1u, m_Height >> (i + 1));

            state.framebuffer = m_Framebuffers[i + 1];
            state.bindings = { m_BindingSets[i] };
            state.viewport.viewports[0] = nvrhi::Viewport(dstWidth, dstHeight);
            state.viewport.scissorRects[0] = nvrhi::Rect(0, dstWidth, 0, dstHeight);

            nvrhi::utils::ClearColorAttachment(ctx.CommandList, m_Framebuffers[i+1], 0, nvrhi::Color(0.0f));

            ctx.CommandList->setGraphicsState(state);
            ctx.CommandList->setPushConstants(&push, sizeof(push));
            ctx.CommandList->draw(nvrhi::DrawArguments().setVertexCount(3));
        }

        // 4. UPSAMPLE CHAIN
        for (int i = m_MipCount - 1; i > 0; i--)
        {
            uint32_t dstWidth = std::max(1u, m_Width >> (i - 1));
            uint32_t dstHeight = std::max(1u, m_Height >> (i - 1));

            state.framebuffer = m_Framebuffers[i-1];
            state.bindings = { m_BindingSets[i] }; // Bind Mip i (Source)
            state.viewport.viewports[0] = nvrhi::Viewport(dstWidth, dstHeight);
            state.viewport.scissorRects[0] = nvrhi::Rect(0, dstWidth, 0, dstHeight);

            // DO NOT Clear! We want to add to the existing Downsample result.

            ctx.CommandList->setGraphicsState(state);
            ctx.CommandList->setPushConstants(&push, sizeof(push));
            ctx.CommandList->draw(nvrhi::DrawArguments().setVertexCount(3));
        }
        renderData.BloomTexture = m_BloomTexture;
        ctx.CommandList->endMarker();
    }

    
}
