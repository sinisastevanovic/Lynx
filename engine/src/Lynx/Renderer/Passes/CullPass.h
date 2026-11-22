#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Decides on the GPU which mesh instances the frame draws. A compute shader tests every instance of
    // the scene against the camera's view and the sun's, and fills in the draw commands that the passes
    // after it draw from. See GpuScene for how a frame is laid out, and Cull.slang.
    class CullPass : public VulkanPass
    {
    public:
        CullPass();
        ~CullPass() override;

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
