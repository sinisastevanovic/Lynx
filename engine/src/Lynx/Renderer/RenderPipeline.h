#pragma once
#include "RenderPass.h"

namespace Lynx
{
    class RenderPipeline
    {
    public:
        void AddPass(std::unique_ptr<RenderPass> pass)
        {
            m_Passes.push_back(std::move(pass));
        }

        void Init(RenderContext& ctx)
        {
            for (auto& pass : m_Passes)
                pass->Init(ctx);
        }

        void Execute(RenderContext& ctx, RenderData& renderData)
        {
            // Sort transparent by distance
            std::ranges::sort(renderData.TransparentQueue,
              [](const RenderCommand& a, const RenderCommand& b) {
                  return a.DistanceToCamera < b.DistanceToCamera; 
              });
            
            for (auto& pass : m_Passes)
                pass->Execute(ctx, renderData);
        }

        void Clear()
        {
            m_Passes.clear();
        }

    private:
        std::vector<std::unique_ptr<RenderPass>> m_Passes;
    };
}