#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // Draws the depth of the scene's opaque meshes ahead of the forward pass, which then shades
    // only the surfaces that end up visible. It runs once per phase of culling: the depth of the early
    // phase is what the late phase is culled against.
    class DepthPass : public VulkanPass
    {
    public:
        explicit DepthPass(CullPhase phase);
        ~DepthPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        CullPhase m_Phase;
        PipelineState m_PipelineState;
    };
}
