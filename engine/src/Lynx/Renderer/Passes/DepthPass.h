#pragma once
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    class DepthPass : public RenderPass
    {
    public:
        virtual ~DepthPass() = default;

        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        nvrhi::BindingLayoutHandle m_GlobalBindingLayout;
        nvrhi::BindingLayoutHandle m_MaterialBindingLayout;
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::GraphicsPipelineHandle m_PipelineMasked;

        // Caching
        nvrhi::BindingSetHandle m_GlobalBindingSet;
        nvrhi::BindingSetHandle m_OpaqueBindingSet;
        nvrhi::BufferHandle m_CachedInstanceBuffer;
        std::unordered_map<Material*, nvrhi::BindingSetHandle> m_MaterialBindingSetCache;

        void CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData);
        nvrhi::BindingSetHandle GetMaterialBindingSet(RenderContext& ctx, Material* material);
    };
}

