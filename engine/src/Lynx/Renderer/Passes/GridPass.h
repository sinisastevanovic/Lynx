#pragma once
#include "Lynx/Renderer/RenderPass.h"

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
        void CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader);

    private:
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::BindingSetHandle m_BindingSet;
        PipelineState m_PipelineState;
    };
}

