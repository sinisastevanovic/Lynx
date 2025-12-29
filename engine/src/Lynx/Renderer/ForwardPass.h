#pragma once
#include "RenderPass.h"

namespace Lynx
{
    class ForwardPass : public RenderPass
    {
    public:
        virtual ~ForwardPass() = default;
        
        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        void CreatePipeline(RenderContext& ctx, nvrhi::IFramebuffer* fb);
        nvrhi::BindingSetHandle GetOrCreateBindingSet(RenderContext& ctx, Material* material, nvrhi::TextureHandle shadowMap, nvrhi::SamplerHandle shadowSampler);

        void DrawQueue(RenderContext& ctx, RenderData& renderData, std::vector<RenderCommand>& queue, nvrhi::GraphicsPipelineHandle pipeline);

    private:
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::GraphicsPipelineHandle m_PipelineOpaque;
        nvrhi::GraphicsPipelineHandle m_PipelineTransparent;
        
        std::unordered_map<Material*, nvrhi::BindingSetHandle> m_BindingSetCache; // TODO: Maybe use AssetHandle??
    };
}

