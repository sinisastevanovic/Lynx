#pragma once
#include "Lynx/Renderer/BindingSetCache.h"
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    class UIPass : public RenderPass
    {
    public:
        virtual ~UIPass() = default;

        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        void CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData);
        void CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader);
        nvrhi::BindingSetHandle GetMaterialBindingSet(RenderContext& ctx, Material* material);

    private:
        nvrhi::BindingLayoutHandle m_GlobalBindingLayout;
        nvrhi::BindingLayoutHandle m_MaterialBindingLayout;
        nvrhi::BindingSetHandle m_GlobalBindingSet;

        BindingSetCache<Material*> m_MaterialBindingSetCache;
        nvrhi::GraphicsPipelineHandle m_Pipeline;

        nvrhi::BufferHandle m_QuadVertexBuffer;
        nvrhi::BufferHandle m_QuadIndexBuffer;
        nvrhi::BufferHandle m_CachedInstanceBuffer;

        PipelineState m_PipelineState;
    };
}

