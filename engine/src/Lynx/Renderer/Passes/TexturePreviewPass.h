#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws one texture into a picture of its own, for a window that shows it: a chosen size of it,
    // chosen channels, a part of it or all (TexturePreview.slang). A window cannot do that by showing
    // the texture itself, which always comes as a whole, in the size that fits, with every channel.
    //
    // Several windows can each show a texture at once, so it draws a picture for every request of a
    // frame (RenderData::Previews), each into a picture kept under the request's key, and says where
    // the texture table has them (RenderData::PreviewPictures). A picture is as large as was asked
    // for, made anew when that changes, and given up a few frames after nobody asked for it.
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
