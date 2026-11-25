#pragma once
#include "SkyInputs.h"
#include "Lynx/Renderer/Graph/RenderGraph.h"

#include <optional>

namespace Lynx
{
    // Draws the picture shiny surfaces mirror the sky from: the sky in every direction, in several
    // sizes, each blurred for a rougher surface than the one before (SkyReflection.slang). Only in a
    // frame in which the sky has changed, after the atmosphere pass, whose sky view it reads.
    // The renderer owns the picture.
    class SkyReflectionPass : public VulkanPass
    {
    public:
        SkyReflectionPass();
        ~SkyReflectionPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        // What the result on the GPU was worked out for. Nothing before the first time and after the
        // shader was reloaded.
        std::optional<SkyInputs> m_Drawn;

        PipelineState m_PipelineState;
    };
}
