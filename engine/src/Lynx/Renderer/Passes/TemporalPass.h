#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Puts the picture together from several frames (Temporal.slang), after everything of the scene is
    // drawn and before exposure, bloom and the composite pass, which then work with its result instead
    // of the frame's own picture.
    //
    // For now it does one thing: with TemporalSettings::Accumulates it adds up the frames of a view that
    // stands still into their average. With jitter on, that is the picture each pixel would have with
    // as many samples as the jitter is long: what anti-aliasing over time is measured against.
    // It starts over whenever the camera moves. What moves in the scene smears, so it is for still scenes.
    class TemporalPass : public VulkanPass
    {
    public:
        TemporalPass();
        ~TemporalPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;

        // Which of the two pictures this frame writes. The other one holds the result of the last frame.
        uint32_t m_Target = 0;

        // How many frames the picture of the last frame is the average of. Zero if there is none to go on from.
        uint32_t m_Accumulated = 0;
    };
}
