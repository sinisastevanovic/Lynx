#pragma once
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    // Turns the finished HDR picture into what the window shows: anti-aliasing, bloom and tone mapping.
    //
    // This is the first pass written directly on Vulkan. It records into the same command buffer as the
    // NVRHI passes before and after it, and uses the textures, shaders and samplers NVRHI created.
    // What that takes is described at the hand-over in Execute.
    class CompositePass : public RenderPass
    {
    public:
        CompositePass();
        ~CompositePass() override;

        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);
        void BindInputs(nvrhi::ITexture* sceneColor, nvrhi::ITexture* bloom);

    private:
        // The Vulkan types stay out of this header, which game code includes through Renderer.h.
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;

        // The textures the descriptor set currently points at.
        nvrhi::TextureHandle m_BoundSceneColor;
        nvrhi::TextureHandle m_BoundBloom;
    };
}
