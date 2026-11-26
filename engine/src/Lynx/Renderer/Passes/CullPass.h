#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Decides on the GPU which mesh instances the frame draws. A compute shader tests every instance of
    // the scene against the camera's view and the sun's, and fills in the draw commands that the passes
    // after it draw from. See GpuScene for how a frame is laid out, and Cull.slang.
    //
    // It runs once per phase, see CullPhase, in three steps, see CullStep. The late phase needs the
    // depth pyramid of what the early phase drew, and without one it lets through everything in view.
    class CullPass : public VulkanPass
    {
    public:
        explicit CullPass(CullPhase phase);
        ~CullPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void UpdatePipelines();
        void CreatePipeline(const Shader& shader, const char* name, size_t pushSize, bool handsOutRuns);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        CullPhase m_Phase;
        PipelineState m_PipelineState;
        PipelineState m_RunPipelineState;
    };
}
