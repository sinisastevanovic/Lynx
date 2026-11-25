#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Sets how bright the picture is made (Exposure.slang). A compute shader counts how bright the
    // scene's picture is into a histogram, and a second step moves the exposure towards the one that
    // suits it, or sets the fixed one. The exposure lives in a small buffer the renderer owns and the
    // scene constants point to: bloom and the composite pass, which run after this one, multiply by it.
    // A view that shows raw colours has no exposure.
    class ExposurePass : public VulkanPass
    {
    public:
        ExposurePass();
        ~ExposurePass() override;

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
