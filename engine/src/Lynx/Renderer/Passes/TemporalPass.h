#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Puts the picture together from several frames (Temporal.slang), after everything of the scene is
    // drawn and before exposure, bloom and the composite pass, which then work with its result instead
    // of the frame's own picture.
    //
    // With TemporalSettings::Resolves it is the anti-aliasing: every pixel mixes the frame with what the
    // frames before left where its surface was, see the shader. It keeps two things from frame to
    // frame, each in a pair of pictures that change roles: the result, with the number of frames it is
    // made of, and the depth of every pixel, to tell whether the history shows
    // the same surface. The views ViewMode::TemporalBlend, TemporalRejected and TemporalClipped show
    // how it worked a frame out, in place of the picture.
    //
    // With TemporalSettings::Accumulates it adds up the frames of a view that stands still into their
    // average instead. With jitter, that is the picture each pixel would have with as many samples as
    // the jitter is long: what the resolve is measured against. It starts over whenever the camera
    // moves. What moves in the scene smears, so it is for still scenes.
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
        void CreatePictures(uint32_t width, uint32_t height);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;

        // Which of each pair of pictures this frame writes. The other one holds what the last frame left.
        uint32_t m_Target = 0;

        // How many frames in a row the pass has run in the way it runs now. Zero if there is nothing
        // of the last frame to go on from.
        uint32_t m_FramesKept = 0;
        bool m_WasAccumulating = false;

        // What Declare decided for the frame.
        bool m_HasHistory = false;
        uint32_t m_View = 0;
    };
}
