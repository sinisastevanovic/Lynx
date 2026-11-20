#pragma once
#include "RenderPass.h"

namespace Lynx
{
    class RenderPipeline
    {
    public:
        // The name identifies the pass in profiler output.
        void AddPass(std::string name, std::unique_ptr<RenderPass> pass)
        {
            m_Passes.push_back({ std::move(name), std::move(pass) });
        }

        void Init(RenderContext& ctx)
        {
            for (auto& entry : m_Passes)
                entry.Pass->Init(ctx);
        }

        // makeScope is called with the name of each pass before it runs, and what it returns is
        // destroyed after the pass has run. That is where a caller measures the pass.
        template<typename MakeScope>
        void Execute(RenderContext& ctx, RenderData& renderData, MakeScope&& makeScope)
        {
            for (auto& entry : m_Passes)
            {
                auto scope = makeScope(entry.Name);
                entry.Pass->Execute(ctx, renderData);
            }
        }

        void Clear()
        {
            m_Passes.clear();
        }

    private:
        struct NamedPass
        {
            std::string Name;
            std::unique_ptr<RenderPass> Pass;
        };
        std::vector<NamedPass> m_Passes;
    };
}