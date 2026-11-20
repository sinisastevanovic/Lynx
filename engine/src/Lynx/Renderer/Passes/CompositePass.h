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
        void CreatePipeline(const Shader& shader);
        void BindInputs(nvrhi::ITexture* sceneColor, nvrhi::ITexture* bloom);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;

        // The textures the descriptor set currently points at.
        nvrhi::TextureHandle m_BoundSceneColor;
        nvrhi::TextureHandle m_BoundBloom;
    };
}
