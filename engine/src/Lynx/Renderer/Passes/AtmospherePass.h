#pragma once
#include "Lynx/Renderer/Atmosphere.h"
#include "Lynx/Renderer/Graph/RenderGraph.h"

#include <optional>

namespace Lynx
{
    // Fills the tables the sky and the sun's colour are read from (Atmosphere.slang): two that depend
    // on the atmosphere alone, which are drawn again only when its settings change, and the view of
    // the sky from the camera, which is drawn every frame because the sun and the camera move.
    // The renderer owns the tables.
    class AtmospherePass : public VulkanPass
    {
    public:
        AtmospherePass();
        ~AtmospherePass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        // The atmosphere the two lasting tables were drawn for. Nothing before the first time and
        // after the shader was reloaded.
        std::optional<AtmosphereSettings> m_Drawn;

        PipelineState m_PipelineState;
    };
}
