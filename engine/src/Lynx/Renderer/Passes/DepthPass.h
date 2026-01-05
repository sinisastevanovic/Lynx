#pragma once
#include "Lynx/Renderer/BindingSetCache.h"
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
        void CreatePipelines(RenderContext& ctx, std::shared_ptr<Shader> shader);
        void CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData);
        nvrhi::BindingSetHandle GetMaterialBindingSet(RenderContext& ctx, Material* material);

    private:
        nvrhi::BindingLayoutHandle m_GlobalBindingLayout;
        nvrhi::BindingLayoutHandle m_MaterialBindingLayout;
        nvrhi::GraphicsPipelineHandle m_Pipeline;

        // Caching
        nvrhi::BindingSetHandle m_GlobalBindingSet;
        nvrhi::BindingSetHandle m_OpaqueBindingSet;
        nvrhi::BufferHandle m_CachedInstanceBuffer;
        BindingSetCache<Material*> m_MaterialBindingSetCache;

        PipelineState m_PipelineState;

        
    };
}

