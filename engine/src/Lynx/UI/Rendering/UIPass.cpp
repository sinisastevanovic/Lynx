#include "UIPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "Lynx/Scene/Components/UIComponents.h"

namespace Lynx
{
    struct UIPushData
    {
        glm::vec2 ScreenSize;
        float PixelRange;
    };

    UIPass::UIPass()
    {
    }

    void UIPass::Init(RenderContext& ctx)
    {
        m_Batcher = std::make_unique<UIBatcher>(ctx.Device);

        // Layout (Set 0)
        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(UIPushData)))
            .setBindingOffsets({ 0, 0, 0, 0 });
          
        m_BindingLayout = ctx.Device->createBindingLayout(layoutDesc);

        m_StandardPipelineState.SetPath("engine/resources/Shaders/UI_Standard.glsl");
        m_StandardPipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            m_StandardPipeline = this->CreatePipeline(ctx, shader);
        });

        m_TextPipelineState.SetPath("engine/resources/Shaders/UI_Text.glsl");
        m_TextPipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            m_TextPipeline = this->CreatePipeline(ctx, shader);
        });
    }

    nvrhi::GraphicsPipelineHandle UIPass::CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader)
    {
        auto pipeDesc = nvrhi::GraphicsPipelineDesc()
            .addBindingLayout(m_BindingLayout)
            .setVertexShader(shader->GetVertexShader())
            .setFragmentShader(shader->GetPixelShader())
            .setPrimType(nvrhi::PrimitiveType::TriangleList);

        pipeDesc.renderState.rasterState.frontCounterClockwise = true; // Quad generation 0->1->2 might be CW
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None; // Safe for 2D

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(UIVertex, Position))
                .setElementStride(sizeof(UIVertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(UIVertex, UV))
                .setElementStride(sizeof(UIVertex)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(UIVertex, Color))
                .setElementStride(sizeof(UIVertex))
        };
        pipeDesc.inputLayout = ctx.Device->createInputLayout(attributes, 3, shader->GetVertexShader());

        // Alpha Blending
        pipeDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)
            .setDestBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha);

        // No Depth Test
        pipeDesc.renderState.depthStencilState.depthTestEnable = false;
        pipeDesc.renderState.depthStencilState.depthWriteEnable = false;

        return ctx.Device->createGraphicsPipeline(pipeDesc, ctx.FinalFramebufferInfo);
    }
    
    void UIPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        // 1. Update Shader if hot-reloaded
        m_StandardPipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            m_StandardPipeline = this->CreatePipeline(ctx, shader);
        });
        m_TextPipelineState.Update([this, &ctx](std::shared_ptr<Shader> shader)
        {
            m_TextPipeline = this->CreatePipeline(ctx, shader);
        });

        // 2. Collect UI Data
        m_Batcher->Begin();

        auto scene = Engine::Get().GetActiveScene();
        if (scene)
        {
            auto view = scene->Reg().view<UICanvasComponent>();
            for (auto entity : view)
            {
                auto& comp = view.get<UICanvasComponent>(entity);
                m_Batcher->Submit(comp.Canvas);
            }
        }

        // 3. Render
        if (m_Batcher->HasData())
        {
            m_Batcher->Upload(ctx.CommandList);

            auto state = nvrhi::GraphicsState()
                .setPipeline(m_StandardPipeline)
                .setFramebuffer(renderData.TargetFramebuffer)
                .addVertexBuffer(nvrhi::VertexBufferBinding(m_Batcher->GetVertexBuffer(), 0, 0))
                .setIndexBuffer(nvrhi::IndexBufferBinding(m_Batcher->GetIndexBuffer(), nvrhi::Format::R32_UINT, 0));

            const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
            state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
            state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));

            UIPushData push;
            push.ScreenSize = { (float)fbInfo.width, (float)fbInfo.height };

            for (const auto& batch : m_Batcher->GetBatches())
            {
                auto pipeline = (batch.Type == UIBatchType::Text) ? m_TextPipeline : m_StandardPipeline;
                state.setPipeline(pipeline);
                auto tex = batch.Texture;
                if (!tex || !tex->GetTextureHandle())
                    continue;

                state.bindings = { GetBindingSet(ctx, tex.get()) };
                ctx.CommandList->setGraphicsState(state);

                push.PixelRange = batch.PixelRange;
                ctx.CommandList->setPushConstants(&push, sizeof(push));
                

                nvrhi::DrawArguments args;
                args.vertexCount = batch.IndexCount;
                args.startIndexLocation = batch.IndexStart;
                ctx.CommandList->drawIndexed(args);

                renderData.DrawCalls++;
                renderData.IndexCount += batch.IndexCount;
            }
        }
    }

    nvrhi::BindingSetHandle UIPass::GetBindingSet(RenderContext& ctx, Texture* texture)
    {
        uint32_t version = texture ? texture->GetVersion() : 0;
        return m_BindingSetCache.Get(texture, version, [&]() -> nvrhi::BindingSetHandle
        {
            // TODO: What sampler do we need? 
            auto desc = nvrhi::BindingSetDesc()
                .addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture->GetTextureHandle()))
                .addItem(nvrhi::BindingSetItem::Sampler(1, SamplerCache::Get()->GetSampler(texture->GetSamplerSettings())));

            return ctx.Device->createBindingSet(desc, m_BindingLayout);
        });
    }
}
