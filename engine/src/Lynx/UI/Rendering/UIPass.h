#pragma once
#include "UIBatcher.h"
#include "Lynx/Renderer/BindingSetCache.h"
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    class UIPass : public RenderPass
    {
    public:
        UIPass();
        virtual ~UIPass() = default;

        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        nvrhi::GraphicsPipelineHandle CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader);
        nvrhi::BindingSetHandle GetBindingSet(RenderContext& ctx, Texture* texture);

    private:
        std::unique_ptr<UIBatcher> m_Batcher;

        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::GraphicsPipelineHandle m_StandardPipeline;
        nvrhi::GraphicsPipelineHandle m_TextPipeline;

        BindingSetCache<Texture*> m_BindingSetCache;

        PipelineState m_StandardPipelineState;
        PipelineState m_TextPipelineState;
    };
}

