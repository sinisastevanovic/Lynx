#include "GridPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    struct GridPushConstants
    {
        glm::mat4 InvView;
        glm::mat4 InvProj;
    };

    void GridPass::Init(RenderContext& ctx)
    {
        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(GridPushConstants)))
            .setBindingOffsets({0, 0, 0, 0});
        m_BindingLayout = ctx.Device->createBindingLayout(layoutDesc);

        auto bsDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, ctx.GlobalConstantBuffer));
        m_BindingSet = ctx.Device->createBindingSet(bsDesc, m_BindingLayout);

        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Grid.glsl");
        LX_ASSERT(shader, "GridPass: Failed to load Grid shader!");

        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.bindingLayouts = { m_BindingLayout };
        pipeDesc.VS = shader->GetVertexShader();
        pipeDesc.PS = shader->GetPixelShader();
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;
        pipeDesc.inputLayout = nullptr;
        pipeDesc.renderState.rasterState.setCullNone();
        pipeDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)
            .setDestBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha);
        pipeDesc.renderState.depthStencilState
            .setDepthTestEnable(true)
            .setDepthWriteEnable(false)
            .setDepthFunc(nvrhi::ComparisonFunc::Less);
        m_Pipeline = ctx.Device->createGraphicsPipeline(pipeDesc, ctx.PresentationFramebufferInfo);
    }

    void GridPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        if (!renderData.ShowGrid)
            return;
        ctx.CommandList->beginMarker("GridPass");

        GridPushConstants push;
        push.InvView = glm::inverse(renderData.View);
        push.InvProj = glm::inverse(renderData.Projection);

        auto state = nvrhi::GraphicsState()
            .setPipeline(m_Pipeline)
            .setFramebuffer(renderData.TargetFramebuffer)
            .addBindingSet(m_BindingSet);

        const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
        state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
        state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));
        ctx.CommandList->setGraphicsState(state);

        ctx.CommandList->setPushConstants(&push, sizeof(GridPushConstants));
        ctx.CommandList->draw(nvrhi::DrawArguments().setVertexCount(6));

        ctx.CommandList->endMarker();

        renderData.DrawCalls++;
        renderData.IndexCount += 6;
    }
}
