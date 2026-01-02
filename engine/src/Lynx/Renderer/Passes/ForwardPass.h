#pragma once
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    class ForwardPass : public RenderPass
    {
    public:
        virtual ~ForwardPass() = default;
        
        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        void CreatePipelines(RenderContext& ctx, std::shared_ptr<Shader> shader);
        nvrhi::BindingSetHandle GetMaterialBindingSet(RenderContext& ctx, Material* material);
        void CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData);
        
        void DrawQueue(RenderContext& ctx, RenderData& renderData, std::vector<RenderCommand>& queue, nvrhi::GraphicsPipelineHandle pipeline);
        void DrawBatches(RenderContext& ctx, RenderData& renderData, std::vector<BatchDrawCall>& batches, nvrhi::GraphicsPipelineHandle pipeline);

    private:
        nvrhi::BindingLayoutHandle m_GlobalBindingLayout;
        nvrhi::BindingLayoutHandle m_MaterialBindingLayout;

        nvrhi::BindingSetHandle m_GlobalBindingSet;

        std::unordered_map<Material*, MaterialCacheEntry> m_MaterialBindingSetCache;
        
        nvrhi::GraphicsPipelineHandle m_PipelineOpaque;
        nvrhi::GraphicsPipelineHandle m_PipelineTransparent;
        nvrhi::BufferHandle m_CachedInstanceBuffer;

        PipelineState m_PipelineState;
    };
}

