#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the editor's ground grid on top of the finished picture, hidden where the scene is in front of it.
    // It comes after post-processing so that anti-aliasing, bloom and tone mapping leave it alone.
    class GridPass : public VulkanPass
    {
    public:
        GridPass();
        ~GridPass() override;

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
