#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws one texture into a picture of its own, for a window that shows it: a chosen size of it,
    // chosen channels, a part of it or all (TexturePreview.slang). A window cannot do that by showing
    // the texture itself, which always comes as a whole, in the size that fits, with every channel.
    //
    // It runs in a frame that asked for it (RenderData::Preview) and leaves the picture in
    // RenderData::PreviewPicture. The picture is as large as was asked for and made anew when that changes.
    class TexturePreviewPass : public VulkanPass
    {
    public:
        TexturePreviewPass();
        ~TexturePreviewPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;
    };
}
