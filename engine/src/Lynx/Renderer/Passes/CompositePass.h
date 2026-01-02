#pragma once
#include "Lynx/Renderer/RenderPass.h"


namespace Lynx
{
    class CompositePass : public RenderPass
    {
    public:
        virtual ~CompositePass() = default;

        virtual void Init(RenderContext& ctx) override;
        virtual void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        void CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader);

    private:
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::BindingSetHandle m_BindingSet;
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::TextureHandle m_CachedInput;
        nvrhi::TextureHandle m_CachedBloom;
        PipelineState m_PipelineState;
    };
}

