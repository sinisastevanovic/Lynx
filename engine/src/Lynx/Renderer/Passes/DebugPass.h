#pragma once
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    class DebugPass : public RenderPass
    {
    public:
        DebugPass() = default;
        virtual ~DebugPass() = default;
        
        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::BindingSetHandle m_BindingSet;

        nvrhi::BufferHandle m_VertexBuffer;

        struct LineVertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
        };
    };
}

