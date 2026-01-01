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
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::BindingSetHandle m_BindingSet;
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::TextureHandle m_CachedInput;
        nvrhi::TextureHandle m_CachedBloom;
    
    };
}

