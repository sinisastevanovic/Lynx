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

    // Draws a number of instances of a submesh from the geometry pool, which has to be bound.
    // firstInstance is where they start in the frame's instance buffer. The shader finds them by it,
    // and their materials through them, so a draw needs no push constants of its own.
    inline void DrawSubmesh(vk::CommandBuffer commands, const Submesh& submesh, uint32_t firstInstance, uint32_t instanceCount)
    {
        commands.drawIndexed(submesh.IndexCount, instanceCount, submesh.Geometry.FirstIndex(), submesh.Geometry.VertexOffset(), firstInstance);
    }
}
