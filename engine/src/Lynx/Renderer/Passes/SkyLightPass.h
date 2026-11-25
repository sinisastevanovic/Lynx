#pragma once
#include "SkyInputs.h"
#include "Lynx/Renderer/Graph/RenderGraph.h"

#include <optional>

namespace Lynx
{
    // Works out the light the sky sends onto surfaces, as nine numbers per colour that the forward
    // shader turns into the light for any way a surface faces (SkyLight.slang). It reads the sky view
    // table, so it runs after the atmosphere pass, and writes a small buffer the renderer owns and
    // the scene constants point to.
    class SkyLightPass : public VulkanPass
    {
    public:
        SkyLightPass();
        ~SkyLightPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        // What the result on the GPU was worked out for. Nothing before the first time and after the
        // shader was reloaded.
        std::optional<SkyInputs> m_Drawn;

        PipelineState m_PipelineState;
    };
}
