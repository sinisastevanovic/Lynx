#include "GraphicsContext.h"

#include "DeviceSelection.h"
#include "Lynx/Log.h"

#include <nvrhi/validation.h>
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// The one place that holds the function pointers vulkan.hpp calls through.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Lynx
{
    namespace
    {
#ifdef _DEBUG
        constexpr bool EnableValidation = true;
#else
        constexpr bool EnableValidation = false;
#endif

        const char* const ValidationLayers[] = { "VK_LAYER_KHRONOS_validation" };

        VKAPI_ATTR VkBool32 VKAPI_CALL LogVulkanMessage(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT, const vk::DebugUtilsMessengerCallbackDataEXT* data, void*)
        {
            if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
                LX_CORE_ERROR("Vulkan Validation Layer: {0}", data->pMessage);
            else if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
                LX_CORE_WARN("Vulkan Validation Layer: {0}", data->pMessage);
            else if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
                LX_CORE_INFO("Vulkan Validation Layer: {0}", data->pMessage);
            else
                LX_CORE_TRACE("Vulkan Validation Layer: {0}", data->pMessage);
            return VK_FALSE;
        }

        // NVRHI reports its own errors, and those of its validation layer, through this.
        class NvrhiMessageLogger : public nvrhi::IMessageCallback
        {
        public:
            void message(nvrhi::MessageSeverity severity, const char* messageText) override
            {
                switch (severity)
                {
                    case nvrhi::MessageSeverity::Info:    LX_CORE_INFO("NVRHI: {0}", messageText); break;
                    case nvrhi::MessageSeverity::Warning: LX_CORE_WARN("NVRHI: {0}", messageText); break;
                    case nvrhi::MessageSeverity::Error:   LX_CORE_ERROR("NVRHI: {0}", messageText); break;
                    case nvrhi::MessageSeverity::Fatal:   LX_CORE_CRITICAL("NVRHI: {0}", messageText); break;
                }
            }
        };

        NvrhiMessageLogger s_NvrhiMessageLogger;
    }

    GraphicsContext::GraphicsContext(GLFWwindow* window)
    {
        const auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(getInstanceProcAddr);

        uint32_t windowExtensionCount = 0;
        const char** windowExtensions = glfwGetRequiredInstanceExtensions(&windowExtensionCount);
        std::vector<const char*> instanceExtensions(windowExtensions, windowExtensions + windowExtensionCount);
        if (EnableValidation)
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        vk::ApplicationInfo applicationInfo;
        applicationInfo.apiVersion = VK_API_VERSION_1_3;

        vk::InstanceCreateInfo instanceInfo;
        instanceInfo.pApplicationInfo = &applicationInfo;
        instanceInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();
        if (EnableValidation)
        {
            instanceInfo.enabledLayerCount = static_cast<uint32_t>(std::size(ValidationLayers));
            instanceInfo.ppEnabledLayerNames = ValidationLayers;
        }

        m_Instance = vk::createInstance(instanceInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Instance);

        if (EnableValidation)
        {
            vk::DebugUtilsMessengerCreateInfoEXT messengerInfo;
            messengerInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            messengerInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            messengerInfo.pfnUserCallback = LogVulkanMessage;
            m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(messengerInfo);
        }

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(m_Instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create window surface!");
        m_Surface = surface;

        auto graphicsDevice = CreateGraphicsDevice(m_Instance, m_Surface);
        if (!graphicsDevice)
        {
            LX_CORE_CRITICAL("{0}", graphicsDevice.error());
            throw std::runtime_error(graphicsDevice.error());
        }

        m_PhysicalDevice = graphicsDevice->PhysicalDevice;
        m_Device = graphicsDevice->Device;
        m_GraphicsQueueFamily = graphicsDevice->GraphicsQueueFamily;
        m_HasMemoryBudget = graphicsDevice->HasMemoryBudget;

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);
        m_GraphicsQueue = m_Device.getQueue(m_GraphicsQueueFamily, 0);

        // The allocator calls Vulkan through the same run-time loaded functions as the rest of the engine.
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.instance = m_Instance;
        allocatorInfo.physicalDevice = m_PhysicalDevice;
        allocatorInfo.device = m_Device;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        if (m_HasMemoryBudget)
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS)
            throw std::runtime_error("Could not create the GPU memory allocator");

        // NVRHI is told what the instance and the device were created with, so it knows what it may use.
        nvrhi::vulkan::DeviceDesc nvrhiDesc;
        nvrhiDesc.errorCB = &s_NvrhiMessageLogger;
        nvrhiDesc.instance = m_Instance;
        nvrhiDesc.physicalDevice = m_PhysicalDevice;
        nvrhiDesc.device = m_Device;
        nvrhiDesc.graphicsQueue = m_GraphicsQueue;
        nvrhiDesc.graphicsQueueIndex = static_cast<int>(m_GraphicsQueueFamily);
        nvrhiDesc.instanceExtensions = instanceExtensions.data();
        nvrhiDesc.numInstanceExtensions = instanceExtensions.size();
        nvrhiDesc.deviceExtensions = graphicsDevice->Extensions.data();
        nvrhiDesc.numDeviceExtensions = graphicsDevice->Extensions.size();
        nvrhiDesc.bufferDeviceAddressSupported = true;

        m_NvrhiVulkanDevice = nvrhi::vulkan::createDevice(nvrhiDesc);
        m_NvrhiDevice = m_NvrhiVulkanDevice;

        // Catches mistakes that the Vulkan validation layer cannot see, such as a binding set that does not match its layout.
        if (EnableValidation)
            m_NvrhiDevice = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
    }

    GraphicsContext::~GraphicsContext()
    {
        m_Device.waitIdle();

        m_NvrhiDevice->runGarbageCollection();
        m_NvrhiDevice = nullptr;
        m_NvrhiVulkanDevice = nullptr;

        // The GPU is idle, so nothing that was retired is still in use.
        m_Deletions.Flush();
        vmaDestroyAllocator(m_Allocator);

        m_Device.destroy();
        m_Instance.destroySurfaceKHR(m_Surface);
        if (m_DebugMessenger)
            m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger);
        m_Instance.destroy();
    }

    std::string GraphicsContext::AdapterName() const
    {
        return std::string(m_PhysicalDevice.getProperties().deviceName.data());
    }

    std::optional<uint64_t> GraphicsContext::VideoMemoryInUse() const
    {
        if (!m_HasMemoryBudget)
            return std::nullopt;

        const auto properties = m_PhysicalDevice.getMemoryProperties2<vk::PhysicalDeviceMemoryProperties2, vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
        const vk::PhysicalDeviceMemoryProperties& memory = properties.get<vk::PhysicalDeviceMemoryProperties2>().memoryProperties;
        const vk::PhysicalDeviceMemoryBudgetPropertiesEXT& budget = properties.get<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();

        // Only the heaps on the graphics card count as video memory.
        uint64_t usedBytes = 0;
        for (uint32_t heap = 0; heap < memory.memoryHeapCount; ++heap)
        {
            if (memory.memoryHeaps[heap].flags & vk::MemoryHeapFlagBits::eDeviceLocal)
                usedBytes += budget.heapUsage[heap];
        }
        return usedBytes;
    }

    void GraphicsContext::WaitIdle() const
    {
        m_Device.waitIdle();
    }
}
