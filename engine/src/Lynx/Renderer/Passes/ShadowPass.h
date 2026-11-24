#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the shadow map of one cascade: the depth of the scene's meshes as the sun sees them in the
    // stretch of the camera's view that the cascade covers. See ShadowCascades.h.
    class ShadowPass : public VulkanPass
    {
    public:
        explicit ShadowPass(uint32_t cascade);
        ~ShadowPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        uint32_t m_Cascade;

        PipelineState m_PipelineState;
    };
}
