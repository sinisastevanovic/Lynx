#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the sky and the sun's disc into the scene's picture wherever the depth buffer is empty.
    // It runs after the depth passes, which have drawn the depth of everything, and before the forward
    // pass, so that see-through things are drawn over the sky. A view that shows raw colours has no sky.
    class SkyPass : public VulkanPass
    {
    public:
        SkyPass();
        ~SkyPass() override;

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
