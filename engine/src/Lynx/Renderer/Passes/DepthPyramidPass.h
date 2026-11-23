#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Builds the depth pyramid from the scene's depth as it stands when the pass runs: the depth in ever
    // smaller sizes, each pixel holding the farthest depth of the pixels it stands for. The culling
    // after it reads the pyramid to leave out what lies behind things that are already drawn.
    class DepthPyramidPass : public VulkanPass
    {
    public:
        DepthPyramidPass();
        ~DepthPyramidPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);
        void EnsurePyramid(uint32_t width, uint32_t height);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;
    };
}
