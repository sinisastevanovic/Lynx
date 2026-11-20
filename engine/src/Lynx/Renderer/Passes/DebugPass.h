#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the frame's debug lines into the scene, hidden where the scene is in front of them.
    class DebugPass : public VulkanPass
    {
    public:
        DebugPass();
        ~DebugPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct LineVertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
        };

        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        PipelineState m_PipelineState;

        // Kept between frames for its memory.
        std::vector<LineVertex> m_Vertices;
    };
}
