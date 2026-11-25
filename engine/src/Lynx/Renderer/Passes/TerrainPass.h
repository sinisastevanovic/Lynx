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
        Colour
    };

    // Draws the patches of the scene's terrains that TerrainRenderer picked for the frame
    // (RenderData::Terrain), with one draw call per kind of patch. See Terrain.slang.
    class TerrainPass : public VulkanPass
    {
    public:
        explicit TerrainPass(TerrainPassKind kind);
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
        PipelineState m_PipelineState;
    };
}
