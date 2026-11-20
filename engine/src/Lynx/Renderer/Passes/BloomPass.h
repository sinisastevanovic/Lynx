#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Makes the glow around bright parts of the scene. The bright parts are copied into a picture of half
    // the size, which is shrunk step by step and then enlarged again, each size added to the one above.
    // The composite pass adds the result to the scene.
    class BloomPass : public VulkanPass
    {
    public:
        BloomPass();
        ~BloomPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipelines(const Shader& shader);
        void EnsureChain(uint32_t width, uint32_t height);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;
        uint32_t m_LinearSampler = 0;
    };
}
