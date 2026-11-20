#pragma once

#include <vulkan/vulkan.hpp>

#include <expected>
#include <string>
#include <vector>

namespace Lynx
{
    // A Vulkan device ready to render with, and what was found out while choosing it.
    struct GraphicsDevice
    {
        vk::PhysicalDevice PhysicalDevice;
        vk::Device Device;

        // A queue family that can draw and can present to the surface the device was chosen for.
        uint32_t GraphicsQueueFamily = 0;

        // The extensions the device was created with. The strings live as long as the program.
        std::vector<const char*> Extensions;

        // Optional abilities. Code that uses one has to check for it.
        bool HasMemoryBudget = false;
        bool HasMeshShaders = false;
        bool HasMinMaxSamplerFilter = false;
        bool HasSmallIntegerStorage = false; // 8-bit and 16-bit values in storage buffers.
    };

    // Looks at every GPU in the machine, picks the best one that has everything the renderer requires
    // and can present to the surface, and creates a device on it with the required features and
    // each optional one the GPU has. The choice and the reasons against the other GPUs are logged.
    // Returns a message naming what is missing if no GPU qualifies.
    std::expected<GraphicsDevice, std::string> CreateGraphicsDevice(vk::Instance instance, vk::SurfaceKHR surface);
}
