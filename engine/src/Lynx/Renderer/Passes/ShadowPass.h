#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the shadow map: the depth of the scene's meshes as the sun sees them.
    class ShadowPass : public VulkanPass
    {
    public:
        explicit ShadowPass(uint32_t resolution);
        ~ShadowPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        uint32_t m_Resolution;

        // Still made through NVRHI, which keeps track of the map's layout between this pass and the forward pass.
        nvrhi::TextureHandle m_ShadowMap;
        nvrhi::SamplerHandle m_ShadowSampler;

        // Where the texture table and the table of comparing samplers have them.
        uint32_t m_ShadowMapIndex = 0;
        uint32_t m_ShadowSamplerIndex = 0;

        PipelineState m_PipelineState;
    };
}
