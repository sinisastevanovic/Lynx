#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the picture shiny surfaces mirror the sky from: the sky in every direction, in several
    // sizes, each blurred for a rougher surface than the one before (SkyReflection.slang). Every frame,
    // after the atmosphere pass, whose sky view it reads. The renderer owns the picture.
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

        PipelineState m_PipelineState;
    };
}
