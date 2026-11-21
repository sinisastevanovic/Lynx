#pragma once

#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Renderer/RenderPass.h"
#include "Lynx/Renderer/Vulkan/GeometryPool.h"

#include <vulkan/vulkan.hpp>

// What the passes that draw meshes share on the C++ side.
namespace Lynx
{
    // The attributes of a mesh vertex, in the order the mesh shaders declare them.
    struct MeshVertexLayout
    {
        static constexpr vk::Format PositionFormat = vk::Format::eR32G32B32Sfloat;
        static constexpr vk::Format NormalFormat = vk::Format::eR32G32B32Sfloat;
        static constexpr vk::Format TangentFormat = vk::Format::eR32G32B32A32Sfloat;
        static constexpr vk::Format TexCoordFormat = vk::Format::eR32G32Sfloat;
        static constexpr vk::Format ColorFormat = vk::Format::eR32G32B32A32Sfloat;
    };

    // Draws a number of instances of a submesh from the geometry pool, which has to be bound.
    inline void DrawSubmesh(vk::CommandBuffer commands, const Submesh& submesh, uint32_t instanceCount)
    {
        commands.drawIndexed(submesh.IndexCount, instanceCount, submesh.Geometry.FirstIndex(), submesh.Geometry.VertexOffset(), 0);
    }

    // The address of an instance in the frame's instance buffer. A draw passes the address of its first
    // instance to the shader, which counts from there.
    inline uint64_t MeshInstanceAddress(const RenderData& renderData, uint32_t instance)
    {
        return renderData.MeshInstanceAddress + instance * sizeof(GPUInstanceData);
    }
}
