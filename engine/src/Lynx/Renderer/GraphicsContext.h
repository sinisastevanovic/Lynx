#pragma once

#include "Vulkan/DeletionQueue.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;
struct VmaAllocator_T;

namespace Lynx
{
    // Everything that exists once per program run and outlives every frame: the Vulkan instance,
    // the window's surface, the GPU, the device on it and its queue, and the NVRHI device on top.
    // It knows nothing about swapchains, frames or scenes.
    class GraphicsContext
    {
    public:
        // Throws std::runtime_error if the machine has no GPU the renderer can run on.
        explicit GraphicsContext(GLFWwindow* window);

        // Everything created from the devices has to be gone before this runs.
        ~GraphicsContext();

        GraphicsContext(const GraphicsContext&) = delete;
        GraphicsContext& operator=(const GraphicsContext&) = delete;

        vk::Instance Instance() const { return m_Instance; }
        vk::SurfaceKHR Surface() const { return m_Surface; }
        vk::PhysicalDevice PhysicalDevice() const { return m_PhysicalDevice; }
        vk::Device Device() const { return m_Device; }
        vk::Queue GraphicsQueue() const { return m_GraphicsQueue; }
        uint32_t GraphicsQueueFamily() const { return m_GraphicsQueueFamily; }

        // The device all rendering code uses. In debug builds it is NVRHI's validation layer,
        // which checks every call against the state NVRHI expects.
        nvrhi::IDevice* Nvrhi() const { return m_NvrhiDevice; }

        // The same device as its Vulkan type, for the calls only that type has.
        nvrhi::vulkan::IDevice* NvrhiVulkan() const { return m_NvrhiVulkanDevice; }

        // Hands out the memory of every buffer the renderer creates itself, see GpuBuffer.
        VmaAllocator_T* Allocator() const { return m_Allocator; }

        // Where GPU objects go when they are no longer wanted but may still be in use by a frame on the GPU.
        DeletionQueue& Deletions() { return m_Deletions; }

        std::string AdapterName() const;

        // How much memory on the graphics card this process uses. Empty if the driver cannot tell.
        std::optional<uint64_t> VideoMemoryInUse() const;

        // Blocks until the GPU has finished everything submitted so far.
        void WaitIdle() const;

    private:
        vk::Instance m_Instance;
        vk::DebugUtilsMessengerEXT m_DebugMessenger;
        vk::SurfaceKHR m_Surface;
        vk::PhysicalDevice m_PhysicalDevice;
        vk::Device m_Device;
        vk::Queue m_GraphicsQueue;
        uint32_t m_GraphicsQueueFamily = 0;
        bool m_HasMemoryBudget = false;

        VmaAllocator_T* m_Allocator = nullptr;
        DeletionQueue m_Deletions;

        nvrhi::vulkan::DeviceHandle m_NvrhiVulkanDevice;
        nvrhi::DeviceHandle m_NvrhiDevice;
    };
}
