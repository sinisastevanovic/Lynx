#pragma once
#include "UIBatcher.h"
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the in-game UI on top of the finished picture: the frame's canvases are turned into
    // one list of triangles, which is drawn in as few calls as the textures allow.
    class UIPass : public VulkanPass
    {
    public:
        UIPass();
        ~UIPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader, UIBatchType type);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        UIBatcher m_Batcher;

        PipelineState m_StandardPipelineState;
        PipelineState m_TextPipelineState;
    };
}
