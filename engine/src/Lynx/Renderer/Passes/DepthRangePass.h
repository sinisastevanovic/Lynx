#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Measures which stretch of the camera's view holds anything: the nearest and the farthest depth of
    // the finished depth buffer. A compute shader finds the two, and the CPU reads them a few frames
    // later, when the GPU has certainly finished that frame (RenderData::SeenDepthRange).
    // Only a perspective is measured.
    //
    // The cascades of the sun's shadow are shared out over that stretch, see ShadowCascades.h.
    class DepthRangePass : public VulkanPass
    {
    public:
        DepthRangePass();
        ~DepthRangePass() override;

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
