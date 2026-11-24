#pragma once

#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Renderer/RenderPass.h"
#include "Lynx/Renderer/Vulkan/GeometryPool.h"

#include <vulkan/vulkan.hpp>

// What the passes that draw meshes share on the C++ side.
namespace Lynx
{
    // How the members of PackedVertex are stored. The GPU unpacks them by these, so a shader declares
    // plain float inputs: three for a normal, four for a tangent, two for a texture coordinate.
    struct MeshVertexLayout
    {
        static constexpr vk::Format PositionFormat = vk::Format::eR32G32B32Sfloat;
        static constexpr vk::Format NormalFormat = vk::Format::eA2B10G10R10SnormPack32;
        static constexpr vk::Format TangentFormat = vk::Format::eA2B10G10R10SnormPack32;
        static constexpr vk::Format TexCoordFormat = vk::Format::eR32G32Sfloat;
    };

    // Draws a run of the frame's draw commands with one call. The geometry pool has to be bound.
    inline void DrawMeshes(vk::CommandBuffer commands, const MeshDrawCommands& draws, DrawRange range)
    {
        if (range.Count == 0)
            return;

        const uint32_t commandSize = sizeof(vk::DrawIndexedIndirectCommand);
        commands.drawIndexedIndirect(draws.Buffer->Handle(), vk::DeviceSize(range.First) * commandSize, range.Count, commandSize);
    }

    // Says which triangles the draws that follow leave out: those that face away, or none.
    // For pipelines made with DynamicCullMode.
    inline void SetCulling(vk::CommandBuffer commands, bool isTwoSided)
    {
        commands.setCullMode(isTwoSided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack);
    }

    // Draws the commands of one view where nothing but depth is drawn. Opaque materials go through a
    // pipeline without a fragment shader, materials with cut-outs through one that reads their texture.
    // The geometry pool has to be bound and the push constants set: both stay as they are when the pipeline changes.
    inline void DrawDepthOfMeshes(vk::CommandBuffer commands, const MeshDrawCommands& draws, const MeshDrawGroups& groups, vk::Pipeline opaquePipeline, vk::Pipeline cutOutPipeline)
    {
        for (uint32_t index = 0; index < MeshDrawKind::Count; ++index)
        {
            const MeshDrawKind kind = MeshDrawKind::At(index);
            const DrawRange range = groups.Of(kind);
            if (range.Count == 0)
                continue;

            commands.bindPipeline(vk::PipelineBindPoint::eGraphics, kind.HasCutOuts ? cutOutPipeline : opaquePipeline);
            SetCulling(commands, kind.IsTwoSided);
            DrawMeshes(commands, draws, range);
        }
    }

    // Draws one see-through instance. The geometry pool has to be bound. The shader finds the instance
    // by its place in the list of drawn instances, and its material through it, so a draw needs no
    // push constants of its own.
    inline void DrawTransparent(vk::CommandBuffer commands, const TransparentDraw& draw)
    {
        commands.drawIndexed(draw.IndexCount, 1, draw.FirstIndex, draw.VertexOffset, draw.FirstInstance);
    }
}
