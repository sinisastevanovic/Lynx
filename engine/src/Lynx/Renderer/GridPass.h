#pragma once
#include "RenderPass.h"

namespace Lynx
{
    class GridPass : public RenderPass
    {
    public:
        GridPass() = default;
        virtual ~GridPass() = default;
        
        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::BindingSetHandle m_BindingSet;
    
    };
}

