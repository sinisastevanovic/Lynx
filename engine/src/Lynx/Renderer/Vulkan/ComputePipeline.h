#pragma once

#include <vulkan/vulkan.hpp>

namespace Lynx
{
    struct ShaderProgram;

    // A pipeline that runs a compute shader: code the GPU runs many times at once on data, without
    // drawing anything. Returns a null handle, after logging why, if the program has no compute stage.
    vk::Pipeline CreateComputePipeline(vk::Device device, const ShaderProgram& program, vk::PipelineLayout layout);

    // How many groups of a given size it takes to cover a number of items. A compute shader runs in
    // groups, and a dispatch names the number of groups, so the last group may reach past the last item:
    // the shader has to check its index against the count.
    constexpr uint32_t GroupCountFor(uint32_t itemCount, uint32_t groupSize)
    {
        return (itemCount + groupSize - 1) / groupSize;
    }
}
