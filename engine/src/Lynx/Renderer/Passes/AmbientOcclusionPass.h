#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // The steps ambient occlusion is worked out in, each a pass of its own in the frame.
    enum class AmbientOcclusionStep
    {
        // The scene's depth as distances from the camera, in five sizes (AmbientOcclusionDepth.slang).
        Depth,

        // How open every pixel is to the sky, from those distances (AmbientOcclusion.slang).
        Occlusion,

        // The same with the coarsest of its grain smoothed out (AmbientOcclusionDenoise.slang).
        // Its result is what the frame uses.
        Denoise
    };

    // The pictures the steps hand each other. One of them belongs to all three passes.
    struct AmbientOcclusionPictures;

    // Ambient occlusion: how much of the sky's light reaches every pixel past what stands around it,
    // worked out from the depth buffer after the depth passes (GTAO, as Intel's XeGTAO does it, see
    // the shaders). The passes that light surfaces read the result at their pixel
    // (RenderData::SceneOcclusion) and dim what comes from the sky by it, never the sun.
    //
    // The last round of smoothing mixes the result with that of the frames before, fetched from
    // where every pixel's surface was (AmbientOcclusionSettings::HistoryTime). The scene's motion is
    // not drawn yet at that point, so the place is worked out from the camera's move alone.
    //
    // It only knows what the depth buffer shows: what is hidden or off screen hides nothing, and only
    // what stands within AmbientOcclusionSettings::Radius counts. It makes corners, creases and
    // the ground under things darker. It does not make a hall dark because it has a roof.
    //
    // A view without perspective has none, and neither has a view of raw colours, which shows no light.
    class AmbientOcclusionPass : public VulkanPass
    {
    public:
        static std::shared_ptr<AmbientOcclusionPictures> MakePictures();

        // The smoothing takes up to MaxSmoothingRounds rounds, each a pass of its own: round says which.
        static constexpr uint32_t MaxSmoothingRounds = 3;
        AmbientOcclusionPass(AmbientOcclusionStep step, std::shared_ptr<AmbientOcclusionPictures> pictures, uint32_t round = 0);
        ~AmbientOcclusionPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        AmbientOcclusionStep m_Step;
        uint32_t m_Round;
        std::shared_ptr<AmbientOcclusionPictures> m_Pictures;
        PipelineState m_PipelineState;
    };
}
