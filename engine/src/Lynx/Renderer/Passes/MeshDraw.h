#pragma once

#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Renderer/RenderPass.h"
#include "Lynx/Renderer/Vulkan/GpuBuffer.h"

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

    // Binds the vertices and indices of a submesh.
    inline void BindSubmesh(vk::CommandBuffer commands, const Submesh& submesh)
    {
        commands.bindVertexBuffers(0, submesh.VertexBuffer.Get().Handle(), vk::DeviceSize(0));
        commands.bindIndexBuffer(submesh.IndexBuffer.Get().Handle(), 0, vk::IndexType::eUint32);
    }

    // The address of an instance in the frame's instance buffer. A draw passes the address of its first
    // instance to the shader, which counts from there.
    inline uint64_t MeshInstanceAddress(const RenderData& renderData, uint32_t instance)
    {
        return renderData.MeshInstanceAddress + instance * sizeof(GPUInstanceData);
    }
}
