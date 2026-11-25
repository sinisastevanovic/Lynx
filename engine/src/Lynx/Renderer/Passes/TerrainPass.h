#pragma once
#include "Lynx/Renderer/Graph/RenderGraph.h"

namespace Lynx
{
    // What a terrain pass draws.
    enum class TerrainPassKind
    {
        // The depth of the terrain, with the meshes the camera saw in the last frame: ground hides
        // more than anything else, so it is part of what later culling tests against.
        Depth,

        // Its lit colour, where its depth is the one in the depth buffer.
        Colour,

        // Its depth as the sun sees it, into the map of one cascade, after the meshes' shadows.
        Shadow
    };

    // Draws the patches of the scene's terrains that TerrainRenderer picked for a view of the frame
    // (RenderData::Terrain), with one draw call per kind of patch. See Terrain.slang.
    class TerrainPass : public VulkanPass
    {
    public:
        // cascade says which cascade of the sun's shadow a shadow pass draws.
        explicit TerrainPass(TerrainPassKind kind, uint32_t cascade = 0);
        ~TerrainPass() override;

        void Init(RenderContext& ctx) override;
        bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) override;
        void Execute(vk::CommandBuffer commands, RenderData& renderData) override;

    private:
        void CreatePipeline(const Shader& shader);

    private:
        struct VulkanObjects;
        std::unique_ptr<VulkanObjects> m_Vulkan;

        TerrainPassKind m_Kind;
        uint32_t m_Cascade;
        PipelineState m_PipelineState;
    };
}
