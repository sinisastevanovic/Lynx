#pragma once
#include "Lynx/Renderer/BindingSetCache.h"
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    class ParticlePass : public RenderPass
    {
    public:
        virtual ~ParticlePass() = default;

        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        void CreatePipelines(RenderContext& ctx, std::shared_ptr<Shader> shader);
        void CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData);
        nvrhi::BindingSetHandle GetMaterialBindingSet(RenderContext& ctx, Material* material);

    private:
        nvrhi::BindingLayoutHandle m_GlobalBindingLayout;
        nvrhi::BindingLayoutHandle m_MaterialBindingLayout;

        nvrhi::BindingSetHandle m_GlobalBindingSet;
        BindingSetCache<Material*> m_MaterialBindingSetCache;

        nvrhi::GraphicsPipelineHandle m_PipelineAlpha;
        nvrhi::GraphicsPipelineHandle m_PipelineAdditive;

        nvrhi::BufferHandle m_QuadVertexBuffer;
        nvrhi::BufferHandle m_QuadIndexBuffer;
        nvrhi::BufferHandle m_CachedInstanceBuffer;

        PipelineState m_PipelineState;
    };
}

