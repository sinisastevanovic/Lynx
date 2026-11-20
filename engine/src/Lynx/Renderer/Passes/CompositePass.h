#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Turns the finished HDR picture into what the window shows: anti-aliasing, bloom and tone mapping.
    // Reads the scene and the bloom, and writes the frame's output.
    class CompositePass : public VulkanPass
    {
    public:
        CompositePass();
        ~CompositePass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        // A texture this pass reads, and where it put it in the texture table.
        struct Input
        {
            nvrhi::TextureHandle Texture;
            uint32_t Index = 0;
        };

        void CreatePipeline(const Shader& shader);
        void UseInput(Input& input, nvrhi::ITexture* texture);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;

        Input m_SceneColor;
        Input m_Bloom;
        uint32_t m_LinearSampler = 0;
    };
}
