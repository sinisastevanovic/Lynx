#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the scene's meshes with their materials, lit by the sun. Opaque meshes come first and are drawn
    // only where the depth pass found them to be visible. See-through meshes follow, farthest first.
    // In the editor, every pixel also gets the ID of its entity.
    class ForwardPass : public VulkanPass
    {
    public:
        ForwardPass();
        ~ForwardPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipelines(const Shader& shader);
        void CreateFoliagePipeline(const Shader& shader);
        void UpdatePipelines();

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        // True in the editor, where the scene has a second target for entity IDs.
        bool m_WritesEntityIds = false;

        PipelineState m_PipelineState;

        // The shaders of foliage, see Foliage.slang.
        PipelineState m_FoliagePipelineState;
    };
}
