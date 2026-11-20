#include "Renderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "ShaderUtils.h"
#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

#include "GpuTimers.h"
#include "SamplerCache.h"
#include "Lynx/Profiling/FrameProfiler.h"
#include "Passes/DebugPass.h"
#include "Passes/ForwardPass.h"
#include "Passes/GridPass.h"
#include "Passes/ShadowPass.h"
#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "nvrhi/validation.h"
#include "Passes/DepthPass.h"
#include "Passes/ParticlePass.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <shaderc/shaderc.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "nvrhi/utils.h"

#include "Lynx/ImGui/imgui_nvrhi.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>


namespace Lynx
{
#ifdef _DEBUG
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;
#endif
    
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT messageTypes, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
            LX_CORE_ERROR("Vulkan Validation Layer: {0}", pCallbackData->pMessage);
        else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
            LX_CORE_WARN("Vulkan Validation Layer: {0}", pCallbackData->pMessage);
        else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
            LX_CORE_INFO("Vulkan Validation Layer: {0}", pCallbackData->pMessage);
        else
            LX_CORE_TRACE("Vulkan Validation Layer: {0}", pCallbackData->pMessage);
        return VK_FALSE;
    }

    namespace Helpers
    {
        static nvrhi::Format TextureFormatToNvrhi(TextureFormat format, bool sRGB)
        {
            switch (format)
            {
                case TextureFormat::RGBA8:
                    return sRGB ? nvrhi::Format::SRGBA8_UNORM : nvrhi::Format::RGBA8_UNORM;
                case TextureFormat::RG16F:
                    return nvrhi::Format::RG16_FLOAT;
                case TextureFormat::RG32F:
                    return nvrhi::Format::RG32_FLOAT;
                case TextureFormat::R32I:
                    return sRGB ? nvrhi::Format::R32_SINT : nvrhi::Format::R32_UINT;
                case TextureFormat::R8:
                    return nvrhi::Format::R8_UNORM;
                case TextureFormat::Depth32:
                    return nvrhi::Format::D32;
                case TextureFormat::Depth24Stencil8:
                    return nvrhi::Format::D24S8;
            }

            return nvrhi::Format::UNKNOWN;
        }

        static uint32_t GetTextureFormatByteSize(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::RGBA8:          return 4;
                case TextureFormat::RG16F:          return 4; // 2 * 2 bytes (half float)
                case TextureFormat::RG32F:          return 8; // 2 * 4 bytes (float)
                case TextureFormat::R32I:           return 4; // 1 * 4 bytes (int)
                case TextureFormat::Depth32:        return 4;
                case TextureFormat::Depth24Stencil8:return 4;
                case TextureFormat::R8:             return 1;
            }
            LX_CORE_WARN("Unknown Texture Format byte size, defaulting to 4");
            return 4;
        }
    }
    
    namespace
    {
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

        // An asset can be loaded in the middle of a frame, while the frame's command list is open.
        // NVRHI allows only one immediate command list to be open at a time, so uploads use one that is not immediate.
        nvrhi::CommandListHandle CreateUploadCommandList(nvrhi::IDevice* device)
        {
            return device->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
        }

        vk::PresentModeKHR ChoosePresentMode(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, bool vsync)
        {
            // FIFO waits for the display refresh, and every Vulkan implementation supports it.
            if (vsync)
                return vk::PresentModeKHR::eFifo;

            // Without VSync, mailbox is preferred because it does not tear. Immediate does.
            const std::vector<vk::PresentModeKHR> supportedModes = physicalDevice.getSurfacePresentModesKHR(surface);
            for (vk::PresentModeKHR preferredMode : { vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate })
            {
                if (std::find(supportedModes.begin(), supportedModes.end(), preferredMode) != supportedModes.end())
                    return preferredMode;
            }

            return vk::PresentModeKHR::eFifo;
        }

        vk::CommandBuffer NativeCommandBuffer(nvrhi::ICommandList* commandList)
        {
            return vk::CommandBuffer(static_cast<VkCommandBuffer>(commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)));
        }

        // Measures what is recorded during its lifetime: the CPU time to record it and the GPU time to run it.
        class PassScope
        {
        public:
            PassScope(FrameProfiler& profiler, GpuTimers& timers, nvrhi::ICommandList* commandList, std::string_view name)
                : m_CpuScope(profiler, name), m_Timers(timers), m_CommandList(commandList), m_Name(name)
            {
                m_Timers.Begin(NativeCommandBuffer(m_CommandList), m_Name);
            }

            ~PassScope()
            {
                m_Timers.End(NativeCommandBuffer(m_CommandList), m_Name);
            }

            PassScope(const PassScope&) = delete;
            PassScope& operator=(const PassScope&) = delete;

        private:
            ProfileScope m_CpuScope;
            GpuTimers& m_Timers;
            nvrhi::ICommandList* m_CommandList;
            std::string_view m_Name;
        };

        // A screenshot is read this many frames after it was copied, when the GPU has certainly finished the copy.
        constexpr uint64_t ScreenshotReadDelay = 3;

        double MillisecondsSince(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    }

    struct Renderer::VulkanState
    {
        vk::Instance Instance;
        vk::SurfaceKHR Surface;
        vk::PhysicalDevice PhysicalDevice;
        vk::Device Device;
        vk::Queue GraphicsQueue;
        uint32_t GraphicsQueueFamily = 0;
        vk::SwapchainKHR Swapchain;
        vk::Format SwapchainFormat;
        vk::Extent2D SwapchainExtent;
        std::vector<vk::Image> SwapchainImages;

        std::vector<vk::Semaphore> ImageAvailableSemaphores;
        std::vector<vk::Semaphore> RenderFinishedSemaphores;
        std::vector<vk::Fence> InFlightFences;
        
        vk::DebugUtilsMessengerEXT DebugMessenger;

        // The NVRHI device as its Vulkan type, for the Vulkan-only calls. m_NvrhiDevice is the same device
        // in release builds, but in debug builds it is the validation layer wrapped around this one.
        nvrhi::vulkan::DeviceHandle NvrhiVulkanDevice;

        std::unique_ptr<GpuTimers> Timers;

        // Whether the driver can report how much memory this process uses per heap.
        bool HasMemoryBudget = false;
    };

    Renderer::Renderer(GLFWwindow* window, FrameProfiler& profiler, bool initIdTarget)
        : m_ShouldCreateIDTarget(initIdTarget), m_Profiler(profiler)
    {
        m_VulkanState = std::make_unique<VulkanState>();
        InitVulkan(window);
        InitNVRHI();
        InitBuffers();
        
        LX_CORE_INFO("Renderer created (Vulkan/NVRHI ready. Pipeline pending.");
    }


    Renderer::~Renderer()
    {
        LX_CORE_INFO("Renderer shutting down...");
        
        if (!m_VulkanState->Device)
            return;

        m_VulkanState->Device.waitIdle();

        if (m_NvrhiDevice)
            m_NvrhiDevice->runGarbageCollection();

        m_PendingScreenshots.clear();
        m_VulkanState->Timers.reset();

        m_Pipeline.Clear();
        m_BloomPass.reset();
        m_CompositePass.reset();
        m_MipMapGenPass.reset();
        m_UIPass.reset();
        m_RenderContext = RenderContext();
        m_CurrentFrameData = RenderData();
        m_OpaqueBatches.clear();
        m_StageBuffer = nullptr;
        m_ParticleInstanceBuffer = nullptr;

        SamplerCache::Shutdown();
        m_WhiteTex = nullptr;
        m_NormalTex = nullptr;
        m_BlackTex = nullptr;
        m_MetallicRoughnessTex = nullptr;
        m_GlobalCB = nullptr;
        m_InstanceBuffer = nullptr;
        m_CommandList = nullptr;
        m_SwapchainFramebuffers.clear();
        m_SceneTarget.reset();
        m_NvrhiDevice = nullptr;
        m_ImGuiBackend.reset();
        m_VulkanState->NvrhiVulkanDevice = nullptr;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            m_VulkanState->Device.destroySemaphore(m_VulkanState->ImageAvailableSemaphores[i]);
            m_VulkanState->Device.destroyFence(m_VulkanState->InFlightFences[i]);
        }

        for (auto& sem : m_VulkanState->RenderFinishedSemaphores) {
            m_VulkanState->Device.destroySemaphore(sem);
        }
        
        m_VulkanState->Device.destroySwapchainKHR(m_VulkanState->Swapchain);
        m_VulkanState->Device.destroy();
        m_VulkanState->Instance.destroySurfaceKHR(m_VulkanState->Surface);

        if (m_VulkanState->DebugMessenger)
            m_VulkanState->Instance.destroyDebugUtilsMessengerEXT(m_VulkanState->DebugMessenger);
        
        m_VulkanState->Instance.destroy();
    }
        
    void Renderer::Init()
    {
        // TODO: Move these members to the struct, useless to have duplicates!
        m_RenderContext.Device = m_NvrhiDevice;
        m_RenderContext.CommandList = m_CommandList;
        m_RenderContext.GlobalConstantBuffer = m_GlobalCB;
        m_RenderContext.WhiteTexture = m_WhiteTex;
        m_RenderContext.BlackTexture = m_BlackTex;
        m_RenderContext.NormalTexture = m_NormalTex;
        m_RenderContext.MetallicRoughnessTexture = m_MetallicRoughnessTex;

        // TODO: Make sure this is up-to-date...
        nvrhi::FramebufferInfo fbInfo;
        fbInfo.addColorFormat(nvrhi::Format::RGBA16_FLOAT);
        if (m_ShouldCreateIDTarget) fbInfo.addColorFormat(nvrhi::Format::R32_SINT);
        fbInfo.setDepthFormat(nvrhi::Format::D32);
        m_RenderContext.PresentationFramebufferInfo = fbInfo;
        nvrhi::FramebufferInfo finalfbInfo;
        finalfbInfo.addColorFormat(nvrhi::Format::BGRA8_UNORM);
        m_RenderContext.FinalFramebufferInfo = finalfbInfo;

        SamplerCache::Init(m_NvrhiDevice, m_MaxAnisotropy);

        m_Pipeline.AddPass("Shadow", std::make_unique<ShadowPass>(m_ShadowMapResolution));
        m_Pipeline.AddPass("Depth", std::make_unique<DepthPass>());
        m_Pipeline.AddPass("Forward", std::make_unique<ForwardPass>());
        m_Pipeline.AddPass("Particles", std::make_unique<ParticlePass>());
        if (m_ShouldCreateIDTarget)
        {
            m_Pipeline.AddPass("Grid", std::make_unique<GridPass>());
        }
        m_Pipeline.AddPass("Debug", std::make_unique<DebugPass>());

        m_Pipeline.Init(m_RenderContext);

        m_BloomPass = std::make_unique<BloomPass>();
        m_BloomPass->Init(m_RenderContext);
        
        m_CompositePass = std::make_unique<CompositePass>();
        m_CompositePass->Init(m_RenderContext);

        m_ImGuiBackend = std::make_unique<ImGui_NVRHI>();
        m_ImGuiBackend->init(m_NvrhiDevice);

        m_MipMapGenPass = std::make_unique<MipMapBlitPass>();
        m_MipMapGenPass->Init(m_NvrhiDevice);

        m_UIPass = std::make_unique<UIPass>();
        m_UIPass->Init(m_RenderContext);
        
        LX_CORE_INFO("Renderer initialized successfully (Pipeline loaded).");
    }

    nvrhi::TextureHandle Renderer::CreateTexture(const TextureSpecification& specification, unsigned char* data)
    {
        uint32_t mipLevels = 1;
        if (specification.GenerateMips)
        {
            mipLevels = (uint32_t)(std::floor(std::log2(std::max(specification.Width, specification.Height))) + 1);
        }
        
        auto desc = nvrhi::TextureDesc()
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setWidth(specification.Width)
            .setHeight(specification.Height)
            .setFormat(Helpers::TextureFormatToNvrhi(specification.Format, specification.IsSRGB))
            .setDebugName(specification.DebugName)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource)
            .setMipLevels(mipLevels)
            .setIsUAV(false)
            .setIsRenderTarget(specification.GenerateMips);

        auto result = m_NvrhiDevice->createTexture(desc);
        if (!result)
        {
            LX_CORE_ERROR("Failed to create NVRHI texture from data");
            return nullptr;
        }

        if (data)
        {
            uint32_t bytesPerPixel = Helpers::GetTextureFormatByteSize(specification.Format);

            size_t rowPitch = specification.Width * bytesPerPixel;
            size_t depthPitch = 0;

            // TODO: Batch this!
            auto cmdList = CreateUploadCommandList(m_NvrhiDevice);
            cmdList->open();
            cmdList->writeTexture(result, 0, 0, data, rowPitch, depthPitch);

            if (specification.GenerateMips)
            {
                m_MipMapGenPass->Generate(cmdList, result);
            }
            
            cmdList->close();
            m_NvrhiDevice->executeCommandList(cmdList);
        }
        
        return result;
    }

    int Renderer::ReadIdFromBuffer(uint32_t x, uint32_t y)
    {
        if (!m_SceneTarget || !m_SceneTarget->IdBuffer)
            return -1;

        if (!m_StageBuffer)
        {
            nvrhi::TextureDesc desc = nvrhi::TextureDesc()
            .setWidth(1)
            .setHeight(1)
            .setFormat(nvrhi::Format::R32_SINT)
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setKeepInitialState(true)
            .setDebugName("ID_Stage");
            m_StageBuffer = m_NvrhiDevice->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
        }

        m_CommandList->open();

        // Define the source region (the pixel under mouse)
        nvrhi::TextureSlice srcSlice;
        srcSlice.x = x;
        srcSlice.y = y;
        srcSlice.z = 0;
        srcSlice.width = 1;
        srcSlice.height = 1;
        srcSlice.depth = 1;
        srcSlice.mipLevel = 0;
        srcSlice.arraySlice = 0;

        // Define dest region (0,0 in staging buffer)
        nvrhi::TextureSlice dstSlice;
        dstSlice.x = 0;
        dstSlice.y = 0;
        dstSlice.z = 0;
        dstSlice.width = 1;
        dstSlice.height = 1;
        dstSlice.depth = 1;
        dstSlice.mipLevel = 0;
        dstSlice.arraySlice = 0;

        m_CommandList->copyTexture(m_StageBuffer, dstSlice, m_SceneTarget->IdBuffer, srcSlice);
        m_CommandList->close();
        m_NvrhiDevice->executeCommandList(m_CommandList);

        // 3. Sync (Wait for copy to finish)
        // TODO: In a real engine, you might want to delay reading by 1-2 frames to avoid stalling.
        // But for an editor tool, a stall is acceptable.
        m_NvrhiDevice->waitForIdle();

        size_t rowPitch;
        void* data = m_NvrhiDevice->mapStagingTexture(m_StageBuffer, dstSlice, nvrhi::CpuAccessMode::Read, &rowPitch);
        
        int entityID = -1;
        if (data)
        {
            entityID = *(int*)data;
            m_NvrhiDevice->unmapStagingTexture(m_StageBuffer);
        }

        return entityID;
    }

    void Renderer::ResetStats()
    {
        m_CurrentFrameData.DrawCalls = 0;
        m_CurrentFrameData.IndexCount = 0;
        memset(&m_Stats, 0, sizeof(RenderStats));
    }

    void Renderer::InitVulkan(GLFWwindow* window)
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(NULL, "vkGetInstanceProcAddr"));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        
        // 1. Instance
        vk::ApplicationInfo appInfo;
        appInfo.apiVersion = VK_API_VERSION_1_3;
        
        vk::InstanceCreateInfo instInfo;
        
        uint32_t count;
        const char** glfweExtensions = glfwGetRequiredInstanceExtensions(&count);
        std::vector<const char*> extensions(glfweExtensions, glfweExtensions + count);

        if (enableValidationLayers)
        {
            instInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instInfo.ppEnabledLayerNames = validationLayers.data();

            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else
        {
            instInfo.enabledLayerCount = 0;
        }

        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instInfo.ppEnabledExtensionNames = extensions.data();
        m_VulkanState->Instance = vk::createInstance(instInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanState->Instance);

        // Setup Debug Messenger
        if (enableValidationLayers)
        {
            vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
            debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | 
                                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
            debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | 
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debugInfo.pfnUserCallback = debugCallback;
            
            m_VulkanState->DebugMessenger = m_VulkanState->Instance.createDebugUtilsMessengerEXT(debugInfo);
        }

        // 2. Surface
        VkSurfaceKHR rawSurface;
        if (glfwCreateWindowSurface(m_VulkanState->Instance, window, nullptr, &rawSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }
        m_VulkanState->Surface = rawSurface;

        // 3. Physical Device
        std::vector<vk::PhysicalDevice> physicalDevices = m_VulkanState->Instance.enumeratePhysicalDevices();
        m_VulkanState->PhysicalDevice = physicalDevices[0];

        // 4. Queue Family
        //m_VulkanState->GraphicsQueueFamily = 0; // Simplified: assuming index 0 has graphics

        // 5. Logical Device
        vk::PhysicalDeviceVulkan13Features features13;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;

        vk::PhysicalDeviceVulkan12Features features12;
        features12.timelineSemaphore = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;
        features12.bufferDeviceAddress = VK_TRUE;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        // The GPU timers reset their queries from the CPU.
        features12.hostQueryReset = VK_TRUE;

        vk::PhysicalDeviceFeatures features10;
        features10.geometryShader = VK_TRUE;
        features10.independentBlend = VK_TRUE;
        features10.samplerAnisotropy = VK_TRUE;

        features13.pNext = &features12;
        
        float priority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo;
        queueInfo.queueFamilyIndex = m_VulkanState->GraphicsQueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        for (const vk::ExtensionProperties& extension : m_VulkanState->PhysicalDevice.enumerateDeviceExtensionProperties())
        {
            if (std::string_view(extension.extensionName) == VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)
            {
                deviceExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
                m_VulkanState->HasMemoryBudget = true;
            }
        }
        vk::DeviceCreateInfo devInfo;
        devInfo.queueCreateInfoCount = 1;
        devInfo.pQueueCreateInfos = &queueInfo;
        devInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        devInfo.ppEnabledExtensionNames = deviceExtensions.data();
        devInfo.pNext = &features13;
        devInfo.pEnabledFeatures = &features10;
        m_VulkanState->Device = m_VulkanState->PhysicalDevice.createDevice(devInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanState->Device);

        m_VulkanState->GraphicsQueue = m_VulkanState->Device.getQueue(m_VulkanState->GraphicsQueueFamily, 0);
        m_VulkanState->Timers = std::make_unique<GpuTimers>(m_VulkanState->PhysicalDevice, m_VulkanState->Device, m_VulkanState->GraphicsQueueFamily);

        // 6. Swapchain, its images and the per-image semaphores
        m_VulkanState->SwapchainFormat = vk::Format::eB8G8R8A8Unorm; // Forced simplified format

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        CreateSwapchain(static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight));

        // 7. Per-frame sync objects
        m_VulkanState->ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_VulkanState->InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        vk::SemaphoreCreateInfo semInfo;
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            m_VulkanState->ImageAvailableSemaphores[i] = m_VulkanState->Device.createSemaphore(semInfo);
            m_VulkanState->InFlightFences[i] = m_VulkanState->Device.createFence(fenceInfo);
        }
    }

    void Renderer::CreateSwapchain(uint32_t fallbackWidth, uint32_t fallbackHeight)
    {
        vk::SurfaceCapabilitiesKHR caps = m_VulkanState->PhysicalDevice.getSurfaceCapabilitiesKHR(m_VulkanState->Surface);

        // Some surfaces leave the extent to the application and report it as UINT32_MAX.
        m_VulkanState->SwapchainExtent = caps.currentExtent;
        if (m_VulkanState->SwapchainExtent.width == UINT32_MAX)
        {
            m_VulkanState->SwapchainExtent.width = fallbackWidth;
            m_VulkanState->SwapchainExtent.height = fallbackHeight;
        }

        vk::SwapchainCreateInfoKHR swapInfo;
        swapInfo.surface = m_VulkanState->Surface;
        swapInfo.minImageCount = 2;
        swapInfo.imageFormat = m_VulkanState->SwapchainFormat;
        swapInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        swapInfo.imageExtent = m_VulkanState->SwapchainExtent;
        swapInfo.imageArrayLayers = 1;
        // Transfer source lets a screenshot be copied out of a finished frame.
        swapInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
        swapInfo.preTransform = caps.currentTransform;
        swapInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapInfo.presentMode = ChoosePresentMode(m_VulkanState->PhysicalDevice, m_VulkanState->Surface, m_VSync);
        swapInfo.clipped = VK_TRUE;

        m_VulkanState->Swapchain = m_VulkanState->Device.createSwapchainKHR(swapInfo);
        LX_CORE_INFO("Swapchain created: {0}x{1}, present mode {2}", swapInfo.imageExtent.width, swapInfo.imageExtent.height, vk::to_string(swapInfo.presentMode));
        m_VulkanState->SwapchainImages = m_VulkanState->Device.getSwapchainImagesKHR(m_VulkanState->Swapchain);

        vk::SemaphoreCreateInfo semInfo;
        m_VulkanState->RenderFinishedSemaphores.resize(m_VulkanState->SwapchainImages.size());
        for (vk::Semaphore& semaphore : m_VulkanState->RenderFinishedSemaphores)
            semaphore = m_VulkanState->Device.createSemaphore(semInfo);
    }

    void Renderer::CreateSwapchainFramebuffers()
    {
        for (auto vkImage : m_VulkanState->SwapchainImages)
        {
            nvrhi::TextureDesc desc;
            desc.width = m_VulkanState->SwapchainExtent.width;
            desc.height = m_VulkanState->SwapchainExtent.height;
            desc.format = nvrhi::Format::BGRA8_UNORM;
            desc.isRenderTarget = true;
            desc.initialState = nvrhi::ResourceStates::Present;
            desc.keepInitialState = true;

            nvrhi::TextureHandle texture = m_NvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, (VkImage)vkImage, desc);
            nvrhi::FramebufferDesc fbDesc;
            fbDesc.addColorAttachment(texture);
            m_SwapchainFramebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
        }
    }

    void Renderer::RecreateSwapchain(uint32_t width, uint32_t height)
    {
        m_VulkanState->Device.waitIdle();

        for (auto& sem : m_VulkanState->RenderFinishedSemaphores)
            m_VulkanState->Device.destroySemaphore(sem);

        m_SwapchainFramebuffers.clear();
        m_CurrentFrameData.TargetFramebuffer = nullptr;
        m_VulkanState->SwapchainImages.clear();

        m_VulkanState->Device.destroySwapchainKHR(m_VulkanState->Swapchain);

        CreateSwapchain(width, height);
        CreateSwapchainFramebuffers();
    }

    void Renderer::SetVSync(bool enabled)
    {
        if (m_VSync == enabled)
            return;

        m_VSync = enabled;

        // The present mode is fixed when a swapchain is created, so changing it needs a new swapchain.
        RecreateSwapchain(m_VulkanState->SwapchainExtent.width, m_VulkanState->SwapchainExtent.height);
    }

    void Renderer::InitNVRHI()
    {
        // 1. Initialize NVRHI on top of Vulkan
        nvrhi::vulkan::DeviceDesc deviceDesc;
        deviceDesc.instance = m_VulkanState->Instance;
        deviceDesc.physicalDevice = m_VulkanState->PhysicalDevice;
        deviceDesc.device = m_VulkanState->Device;
        deviceDesc.graphicsQueue = m_VulkanState->GraphicsQueue;
        deviceDesc.graphicsQueueIndex = m_VulkanState->GraphicsQueueFamily;
        deviceDesc.errorCB = &s_NvrhiMessageLogger;

        // TODO: Pass validation extensions
        m_VulkanState->NvrhiVulkanDevice = nvrhi::vulkan::createDevice(deviceDesc);
        m_NvrhiDevice = m_VulkanState->NvrhiVulkanDevice;

        // Checks every NVRHI call against the state NVRHI expects, which catches mistakes
        // that the Vulkan validation layer cannot see, such as a binding set that does not match its layout.
        if (enableValidationLayers)
            m_NvrhiDevice = nvrhi::validation::createValidationLayer(m_NvrhiDevice);

        CreateSwapchainFramebuffers();

        // 3. Create Command List
        m_CommandList = m_NvrhiDevice->createCommandList();

        // 4. Create Default White Texture
        nvrhi::TextureDesc defaultTexDesc;
        defaultTexDesc.width = 1;
        defaultTexDesc.height = 1;
        defaultTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultTexDesc.debugName = "DefaultWhite";
        defaultTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultTexDesc.keepInitialState = true;
        m_WhiteTex = m_NvrhiDevice->createTexture(defaultTexDesc);

        nvrhi::TextureDesc defaultNormalTexDesc;
        defaultNormalTexDesc.width = 1;
        defaultNormalTexDesc.height = 1;
        defaultNormalTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultNormalTexDesc.debugName = "DefaultNormal";
        defaultNormalTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultNormalTexDesc.keepInitialState = true;
        m_NormalTex = m_NvrhiDevice->createTexture(defaultNormalTexDesc);

        nvrhi::TextureDesc defaultMrTexDesc;
        defaultMrTexDesc.width = 1;
        defaultMrTexDesc.height = 1;
        defaultMrTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultMrTexDesc.debugName = "DefaultMetallicRoughness";
        defaultMrTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultMrTexDesc.keepInitialState = true;
        m_MetallicRoughnessTex = m_NvrhiDevice->createTexture(defaultMrTexDesc);
        
        nvrhi::TextureDesc defaultBlackTexDesc;
        defaultBlackTexDesc.width = 1;
        defaultBlackTexDesc.height = 1;
        defaultBlackTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultBlackTexDesc.debugName = "DefaultBlack";
        defaultBlackTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultBlackTexDesc.keepInitialState = true;
        m_BlackTex = m_NvrhiDevice->createTexture(defaultBlackTexDesc);

        uint32_t whitePixel = 0xFFFFFFFF;
        uint32_t normalPixel = 0xFFFF8080;
        uint32_t mrPixel = 0xFF8080FF;
        uint32_t blackPixel = 0xFF000000;
        m_CommandList->open();
        m_CommandList->writeTexture(m_WhiteTex, 0, 0, &whitePixel, 4);
        m_CommandList->writeTexture(m_NormalTex, 0, 0, &normalPixel, 4);
        m_CommandList->writeTexture(m_BlackTex, 0, 0, &blackPixel, 4);
        m_CommandList->writeTexture(m_MetallicRoughnessTex, 0, 0, &mrPixel, 4);
        m_CommandList->close();
        m_NvrhiDevice->executeCommandList(m_CommandList);
    }
    
    void Renderer::InitBuffers()
    {
        nvrhi::BufferDesc cbDesc;
        cbDesc.byteSize = sizeof(SceneData);
        cbDesc.isConstantBuffer = true;
        cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        cbDesc.keepInitialState = true;
        m_GlobalCB = m_NvrhiDevice->createBuffer(cbDesc);

        if (!m_GlobalCB)
        {
            LX_CORE_ERROR("Failed to create Constant Buffer!");
        }
    }
    
    void Renderer::CreateRenderTarget(RenderTarget& target, uint32_t width, uint32_t height)
    {
        target.Width = width;
        target.Height = height;

        auto colorDesc = nvrhi::TextureDesc()
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::RGBA16_FLOAT)
            .setIsRenderTarget(true)
            .setDebugName("SceneHDR")
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true);
        target.Color = m_NvrhiDevice->createTexture(colorDesc);

        auto depthDesc = nvrhi::TextureDesc()
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::D32)
            .setIsRenderTarget(true)
            .setDebugName("SceneDepth")
            .setInitialState(nvrhi::ResourceStates::DepthWrite)
            .setKeepInitialState(true);
        target.Depth = m_NvrhiDevice->createTexture(depthDesc);

        auto outputDesc = nvrhi::TextureDesc()
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::BGRA8_UNORM)
            .setIsRenderTarget(true)
            .setDebugName("SceneLDR")
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true);
        target.Output = m_NvrhiDevice->createTexture(outputDesc);

        if (m_ShouldCreateIDTarget)
        {
            auto idDesc = nvrhi::TextureDesc()
                .setWidth(width)
                .setHeight(height)
                .setFormat(nvrhi::Format::R32_SINT)
                .setIsRenderTarget(true)
                .setDebugName("OffscreenId")
                .setInitialState(nvrhi::ResourceStates::RenderTarget)
                .setKeepInitialState(true)
                .setClearValue(nvrhi::Color(-1.0f, 0.0f, 0.0f, 0.0f));
            target.IdBuffer = m_NvrhiDevice->createTexture(idDesc);
        }

        auto hdrFBDesc = nvrhi::FramebufferDesc()
            .addColorAttachment(target.Color)
            .setDepthAttachment(target.Depth);
        if (m_ShouldCreateIDTarget)
            hdrFBDesc.addColorAttachment(target.IdBuffer);
        target.HDRFramebuffer = m_NvrhiDevice->createFramebuffer(hdrFBDesc);

        auto ldrFBDesc = nvrhi::FramebufferDesc()
            .addColorAttachment(target.Output);
        target.LDRFramebuffer = m_NvrhiDevice->createFramebuffer(ldrFBDesc);
    }

    void Renderer::PrepareDrawCalls()
    {
        std::vector<GPUInstanceData> allInstanceData;
        m_CurrentFrameData.OpaqueDrawCalls.clear();

        // Flatten all batches into single array
        for (auto& [key, instances] : m_OpaqueBatches)
        {
            if (instances.empty())
                continue;

            uint32_t startOffset = (uint32_t)allInstanceData.size();
            allInstanceData.insert(allInstanceData.end(), instances.begin(), instances.end());

            m_CurrentFrameData.OpaqueDrawCalls.push_back({ key, startOffset, (uint32_t)instances.size() });
        }

        std::ranges::sort(m_CurrentFrameData.TransparentQueue,
          [](const RenderCommand& a, const RenderCommand& b) {
              return a.DistanceToCamera > b.DistanceToCamera; 
          });

        for (auto& cmd : m_CurrentFrameData.TransparentQueue)
        {
            cmd.InstanceOffset = (int)allInstanceData.size();
            allInstanceData.push_back({ cmd.InstanceData.Model, cmd.InstanceData.EntityID });
        }

        // Create or resize GPU Buffer
        size_t requiredSize = allInstanceData.size() * sizeof(GPUInstanceData);
        if (requiredSize > 0)
        {
            if (!m_InstanceBuffer || m_InstanceBuffer->getDesc().byteSize < requiredSize)
            {
                nvrhi::BufferDesc desc;
                desc.byteSize = (uint64_t)(requiredSize * 1.5);
                desc.structStride = sizeof(GPUInstanceData);
                desc.debugName = "InstanceBuffer";
                desc.initialState = nvrhi::ResourceStates::ShaderResource;
                desc.keepInitialState = true;
                m_InstanceBuffer = m_NvrhiDevice->createBuffer(desc);
            }

            m_CommandList->writeBuffer(m_InstanceBuffer, allInstanceData.data(), requiredSize);
        }

        m_CurrentFrameData.InstanceBuffer = m_InstanceBuffer;
        m_Profiler.SetCounter("Instances", static_cast<double>(allInstanceData.size()));

        std::vector<ParticleInstanceData> allParticleData;
        m_CurrentFrameData.ParticleQueue.clear();

        for (auto& [material, particles] : m_ParticleBatches)
        {
            if (particles.empty())
                continue;

            ParticleBatch batch;
            batch.Material = material;
            batch.StartOffset = (uint32_t)allParticleData.size();
            batch.Count = (uint32_t)particles.size();

            m_CurrentFrameData.ParticleQueue.push_back(batch);
            allParticleData.insert(allParticleData.end(), particles.begin(), particles.end());
        }

        size_t requiredParticleSize = allParticleData.size() * sizeof(ParticleInstanceData);
        if (requiredParticleSize > 0)
        {
            if (!m_ParticleInstanceBuffer || m_ParticleInstanceBuffer->getDesc().byteSize < requiredParticleSize)
            {
                nvrhi::BufferDesc desc;
                desc.byteSize = (uint64_t)(requiredParticleSize * 1.5);
                desc.structStride = sizeof(ParticleInstanceData);
                desc.debugName = "ParticleInstanceBuffer";
                desc.initialState = nvrhi::ResourceStates::ShaderResource;
                desc.keepInitialState = true;
                m_ParticleInstanceBuffer = m_NvrhiDevice->createBuffer(desc);
            }

            m_CommandList->writeBuffer(m_ParticleInstanceBuffer, allParticleData.data(), requiredParticleSize);
        }
        m_CurrentFrameData.ParticleInstanceBuffer = m_ParticleInstanceBuffer;
    }

    void Renderer::BeginScene(const glm::mat4& view, const glm::mat4 projection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity, float deltaTime, bool editMode)
    {
        ResetStats();
        m_Stats.FrameTime = deltaTime;

        {
            // Waiting here is where the CPU is held back by the GPU or, with VSync, by the display.
            ProfileScope waitScope(m_Profiler, "WaitForGpu");

            // 0. Wait for Fence
            if (m_VulkanState->Device.waitForFences(1, &m_VulkanState->InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
            {
                 LX_CORE_ERROR("Failed to wait for fence!");
            }
            m_VulkanState->Device.resetFences(1, &m_VulkanState->InFlightFences[m_CurrentFrame]);

            // 1. Acquire Image from Vulkan
            // TODO: handle resizing (VK_ERROR_OUT_OF_DATE_KHR)
            auto result = m_VulkanState->Device.acquireNextImageKHR(
                m_VulkanState->Swapchain,
                UINT64_MAX,
                vk::Semaphore(m_VulkanState->ImageAvailableSemaphores[m_CurrentFrame]),
                nullptr
            );

            m_CurrentImageIndex = result.value;
        }

        // 2. Start recording
        m_CommandList->open();
        m_VulkanState->Timers->BeginFrame(m_Profiler);
        m_VulkanState->Timers->Begin(NativeCommandBuffer(m_CommandList), "Frame");

        m_OpaqueBatches.clear();
        m_ParticleBatches.clear();
        m_CurrentFrameData.OpaqueDrawCalls.clear();
        m_CurrentFrameData.TransparentQueue.clear();
        m_CurrentFrameData.ParticleQueue.clear();
        
        glm::vec3 center = cameraPosition;

        glm::vec3 lightDirNorm = glm::normalize(lightDir);
        glm::vec3 lightPos = center - lightDirNorm * 100.0f; // Move back enough to not clip near objects
        
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(lightDirNorm, up)) > 0.99f)
            up = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::mat4 lightView = glm::lookAt(lightPos, center, up);

        float orthoSize = 100.0f;
        float zNear = 0.1f;
        float zFar = 1000.0f;

        glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, zNear, zFar);
        
        // Flip Y for Vulkan (Y is down in clip space)
        // Handled in Shader Bias Matrix now
        // lightProj[1][1] *= -1.0f; 

        glm::mat4 lightViewProj = lightProj * lightView;

        // Shadow Map Stabilization
        // We need to snap the projection to the nearest texel in Light Space to avoid shimmering
        float shadowMapRes = (float)m_ShadowMapResolution;
        
        // Project a reference point (Origin) to Shadow Map NDC Space
        glm::vec4 shadowOrigin = lightViewProj * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin /= shadowOrigin.w; 

        // NDC is [-1, 1] for XY. Resolution is [0, Res].
        float texelSizeNDC = 2.0f / shadowMapRes;

        // Snap to grid
        float snappedX = round(shadowOrigin.x / texelSizeNDC) * texelSizeNDC;
        float snappedY = round(shadowOrigin.y / texelSizeNDC) * texelSizeNDC;

        // Calculate offset
        float dX = snappedX - shadowOrigin.x;
        float dY = snappedY - shadowOrigin.y;

        // Apply offset to the Projection Matrix
        lightViewProj[3][0] += dX;
        lightViewProj[3][1] += dY;

        m_CurrentFrameData.View = view;
        m_CurrentFrameData.Projection = projection;
        m_CurrentFrameData.ViewProjection = projection * view;
        m_CurrentFrameData.CameraPosition = cameraPosition;
        m_CurrentFrameData.LightDirection = lightDir;
        m_CurrentFrameData.LightColor = lightColor;
        m_CurrentFrameData.LightIntensity = lightIntensity;
        m_CurrentFrameData.LightViewProj = lightViewProj;
        m_CurrentFrameData.ShowGrid = editMode && m_ShowGrid;
        if (!m_SceneTarget)
        {
            auto [w, h] = GetViewportSize();
            m_SceneTarget = std::make_unique<RenderTarget>();
            CreateRenderTarget(*m_SceneTarget, w, h);
        }

        m_CurrentFrameData.TargetFramebuffer = m_SceneTarget->HDRFramebuffer;
        m_CurrentFrameData.SceneColorInput = m_SceneTarget->Color;
        if (m_SceneTarget->IdBuffer)
            m_CommandList->clearTextureUInt(m_SceneTarget->IdBuffer, nvrhi::AllSubresources, (uint32_t)-1);
        
        SceneData sceneData;
        sceneData.ViewProjectionMatrix = m_CurrentFrameData.ViewProjection;
        sceneData.LightViewProjection = lightViewProj;
        sceneData.CameraPosition = glm::vec4(cameraPosition, 1.0f);
        sceneData.LightDirection = glm::vec4(lightDir, lightIntensity);
        sceneData.LightColor = glm::vec4(lightColor, 1.0f);
        m_CommandList->writeBuffer(m_GlobalCB, &sceneData, sizeof(SceneData));

        // 3. Clear Screen
        nvrhi::utils::ClearColorAttachment(m_CommandList, m_CurrentFrameData.TargetFramebuffer, 0, nvrhi::Color(0.05f, 0.05f, 0.05f, 1.0f));
        nvrhi::utils::ClearDepthStencilAttachment(m_CommandList, m_CurrentFrameData.TargetFramebuffer, 1.0f, 0);

        m_SubmitStart = std::chrono::steady_clock::now();
    }

    void Renderer::SubmitMesh(std::shared_ptr<StaticMesh> mesh, const glm::mat4& transform, RenderFlags Flags, int entityID)
    {
        if (!mesh)
            return;

        float dist = glm::distance(m_CurrentFrameData.CameraPosition, glm::vec3(transform[3]));

        for (int i = 0; i < mesh->GetSubmeshes().size(); ++i)
        {
            const auto& submesh = mesh->GetSubmeshes()[i];
            if (!submesh.Material)
                continue;

            GPUInstanceData instance = { transform, entityID };
            BatchKey key = { mesh.get(), (uint32_t)i, submesh.Material.get(), Flags };

            if (submesh.Material->Mode == AlphaMode::Translucent)
            {
                RenderCommand cmd;
                cmd.Mesh = mesh;
                cmd.SubmeshIndex = i;
                cmd.InstanceData = instance;
                cmd.DistanceToCamera = dist;
                cmd.Flags = Flags;
                m_CurrentFrameData.TransparentQueue.push_back(cmd);
            }
            else
            {
                m_OpaqueBatches[key].push_back(instance);
            }
        }
        // 1. Bind Texture
        // Check if mesh has a texture. If so, bind it. If not, use white texture.
        // NOTE: This is expensive to do per-draw-call (creating BindingSet).
        // Optimization for later: Cache BindingSets in the Mesh or a Material.

        // For now, let's reuse SetTexture logic or just call it here.
        // We need to access the AssetManager to get the texture by UUID,
        // OR just assume Mesh holds the Texture pointer directly?
        // You implemented Mesh. Does it hold UUID or shared_ptr?
        // If UUID, we need AssetManager. If ptr, easy.

        // Let's assume for V1 we just use the currently bound texture (from SetTexture)
        // OR we implement a simple lookup.
    }

    void Renderer::SubmitParticles(Material* material, const std::vector<ParticleInstanceData>& particles)
    {
        if (!material || particles.empty())
            return;

        auto& batch = m_ParticleBatches[material];
        batch.insert(batch.end(), particles.begin(), particles.end());
    }

    void Renderer::EndScene()
    {
        // Everything between BeginScene and here is the scene handing its meshes to the renderer.
        m_Profiler.AddCpuTime("Submit", MillisecondsSince(m_SubmitStart));

        GpuTimers& timers = *m_VulkanState->Timers;
        const auto measurePass = [this, &timers](std::string_view name)
        {
            return PassScope(m_Profiler, timers, m_CommandList, name);
        };

        {
            ProfileScope prepareScope(m_Profiler, "PrepareDraws");
            PrepareDrawCalls();
        }

        m_Pipeline.Execute(m_RenderContext, m_CurrentFrameData, measurePass);

        m_Stats.DrawCalls = m_CurrentFrameData.DrawCalls;
        m_Stats.IndexCount = m_CurrentFrameData.IndexCount;
        m_Profiler.SetCounter("DrawCalls", m_Stats.DrawCalls);
        m_Profiler.SetCounter("Triangles", m_Stats.IndexCount / 3);

        m_CurrentFrameData.SceneColorInput = m_SceneTarget->Color;
        m_CurrentFrameData.FXAAEnabled = m_FXAAEnabled;
        {
            auto scope = measurePass("Bloom");
            m_BloomPass->Execute(m_RenderContext, m_CurrentFrameData);
        }

        if (m_ShouldCreateIDTarget)
        {
            m_CurrentFrameData.TargetFramebuffer = m_SceneTarget->LDRFramebuffer;
        }
        else
        {
            m_CurrentFrameData.TargetFramebuffer = m_SwapchainFramebuffers[m_CurrentImageIndex];
        }

        {
            auto scope = measurePass("Composite");
            m_CompositePass->Execute(m_RenderContext, m_CurrentFrameData);
        }

        if (m_ShowUI)
        {
            auto scope = measurePass("UI");
            m_UIPass->Execute(m_RenderContext, m_CurrentFrameData);
        }

        if (!m_ScreenshotRequests.empty())
            CopyFrameForScreenshots(m_CurrentFrameData.TargetFramebuffer->getDesc().colorAttachments[0].texture);

        timers.End(NativeCommandBuffer(m_CommandList), "Frame");

        // Submitting and presenting, including the ImGui layer.
        ProfileScope presentScope(m_Profiler, "Present");

        // 1. Close recording
        m_CommandList->close();

        // 2. Execute
        nvrhi::vulkan::IDevice* vkDevice = m_VulkanState->NvrhiVulkanDevice;

        // SYNC: Wait for the image to be available before executing
        vkDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, (VkSemaphore)m_VulkanState->ImageAvailableSemaphores[m_CurrentFrame], 0);

        // SYNC: Signal RenderFinished after executing
        vkDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, (VkSemaphore)m_VulkanState->RenderFinishedSemaphores[m_CurrentImageIndex], 0);

        m_NvrhiDevice->executeCommandList(m_CommandList);

        m_ImGuiBackend->render(m_SwapchainFramebuffers[m_CurrentImageIndex]);

        // Signal Fence for CPU Sync
        vk::SubmitInfo submitInfo;
        m_VulkanState->GraphicsQueue.submit(submitInfo, m_VulkanState->InFlightFences[m_CurrentFrame]);

        m_NvrhiDevice->runGarbageCollection();

        // 3. Present to Screen
        vk::PresentInfoKHR presentInfo;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_VulkanState->Swapchain;
        presentInfo.pImageIndices = &m_CurrentImageIndex;
        
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_VulkanState->RenderFinishedSemaphores[m_CurrentImageIndex];
        
        m_VulkanState->GraphicsQueue.presentKHR(&presentInfo);

        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        ReportMemoryUse();
    }

    void Renderer::RequestScreenshot(uint64_t tag)
    {
        m_ScreenshotRequests.push_back(tag);
    }

    void Renderer::CopyFrameForScreenshots(nvrhi::ITexture* frame)
    {
        const nvrhi::TextureDesc& frameDesc = frame->getDesc();

        for (uint64_t tag : m_ScreenshotRequests)
        {
            nvrhi::TextureDesc stagingDesc;
            stagingDesc.width = frameDesc.width;
            stagingDesc.height = frameDesc.height;
            stagingDesc.format = frameDesc.format;
            stagingDesc.debugName = "Screenshot";

            PendingScreenshot pending;
            pending.Tag = tag;
            pending.Frame = m_Profiler.CurrentFrame();
            pending.Staging = m_NvrhiDevice->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);

            m_CommandList->copyTexture(pending.Staging, nvrhi::TextureSlice(), frame, nvrhi::TextureSlice());
            m_PendingScreenshots.push_back(std::move(pending));
        }

        m_ScreenshotRequests.clear();
    }

    std::vector<Screenshot> Renderer::TakeScreenshots(bool waitForAll)
    {
        if (waitForAll && !m_PendingScreenshots.empty())
            m_NvrhiDevice->waitForIdle();

        std::vector<Screenshot> finished;
        const uint64_t currentFrame = m_Profiler.CurrentFrame();

        for (auto pending = m_PendingScreenshots.begin(); pending != m_PendingScreenshots.end();)
        {
            if (!waitForAll && currentFrame < pending->Frame + ScreenshotReadDelay)
            {
                ++pending;
                continue;
            }

            const nvrhi::TextureDesc& desc = pending->Staging->getDesc();

            size_t rowPitch = 0;
            const auto* source = static_cast<const uint8_t*>(m_NvrhiDevice->mapStagingTexture(pending->Staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch));
            if (source)
            {
                Screenshot screenshot;
                screenshot.Tag = pending->Tag;
                screenshot.Width = desc.width;
                screenshot.Height = desc.height;
                screenshot.Rgba.resize(static_cast<size_t>(desc.width) * desc.height * 4);

                // The frame is BGRA. Its alpha channel does not describe coverage, so the picture is made opaque.
                for (uint32_t y = 0; y < desc.height; ++y)
                {
                    const uint8_t* sourceRow = source + y * rowPitch;
                    uint8_t* targetRow = screenshot.Rgba.data() + static_cast<size_t>(y) * desc.width * 4;
                    for (uint32_t x = 0; x < desc.width; ++x)
                    {
                        targetRow[x * 4 + 0] = sourceRow[x * 4 + 2];
                        targetRow[x * 4 + 1] = sourceRow[x * 4 + 1];
                        targetRow[x * 4 + 2] = sourceRow[x * 4 + 0];
                        targetRow[x * 4 + 3] = 255;
                    }
                }

                m_NvrhiDevice->unmapStagingTexture(pending->Staging);
                finished.push_back(std::move(screenshot));
            }
            else
            {
                LX_CORE_ERROR("Could not read back screenshot {0}.", pending->Tag);
            }

            pending = m_PendingScreenshots.erase(pending);
        }

        return finished;
    }

    void Renderer::FlushGpuTimers()
    {
        m_NvrhiDevice->waitForIdle();
        m_VulkanState->Timers->Flush(m_Profiler);
    }

    std::string Renderer::GetAdapterName() const
    {
        const vk::PhysicalDeviceProperties properties = m_VulkanState->PhysicalDevice.getProperties();
        return std::string(properties.deviceName.data());
    }

    void Renderer::ReportMemoryUse()
    {
        if (!m_VulkanState->HasMemoryBudget)
            return;

        const auto properties = m_VulkanState->PhysicalDevice.getMemoryProperties2<vk::PhysicalDeviceMemoryProperties2, vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
        const vk::PhysicalDeviceMemoryProperties& memory = properties.get<vk::PhysicalDeviceMemoryProperties2>().memoryProperties;
        const vk::PhysicalDeviceMemoryBudgetPropertiesEXT& budget = properties.get<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();

        // Only the heaps on the graphics card count as video memory.
        uint64_t usedBytes = 0;
        for (uint32_t heap = 0; heap < memory.memoryHeapCount; ++heap)
        {
            if (memory.memoryHeaps[heap].flags & vk::MemoryHeapFlagBits::eDeviceLocal)
                usedBytes += budget.heapUsage[heap];
        }

        m_Profiler.SetCounter("VideoMemoryMB", static_cast<double>(usedBytes) / (1024.0 * 1024.0));
    }

    std::pair<nvrhi::BufferHandle, nvrhi::BufferHandle> Renderer::CreateMeshBuffers(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
    {
        auto vbDesc = nvrhi::BufferDesc()
           .setByteSize(vertices.size() * sizeof(Vertex))
           .setIsVertexBuffer(true)
           .setDebugName("MeshVertexBuffer")
           .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::BufferHandle vb = m_NvrhiDevice->createBuffer(vbDesc);

        auto ibDesc = nvrhi::BufferDesc()
            .setByteSize(indices.size() * sizeof(uint32_t))
            .setIsIndexBuffer(true)
            .setDebugName("MeshIndexBuffer")
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::BufferHandle ib = m_NvrhiDevice->createBuffer(ibDesc);

        // TODO: We use a temporary command list here to immediately upload the buffers,
        // in the future, we should maybe batch these things? 
        nvrhi::CommandListHandle commandList = CreateUploadCommandList(m_NvrhiDevice);
        commandList->open();
        commandList->writeBuffer(vb, vertices.data(), vbDesc.byteSize);
        commandList->writeBuffer(ib, indices.data(), ibDesc.byteSize);
        commandList->close();
        m_NvrhiDevice->executeCommandList(commandList);

        return { vb, ib };
    }

    void Renderer::OnResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) return;

        RecreateSwapchain(width, height);

    }

    void Renderer::EnsureEditorViewport(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        if (!m_SceneTarget)
        {
            m_SceneTarget = std::make_unique<RenderTarget>();
            CreateRenderTarget(*m_SceneTarget, width, height);
        }
        else if (m_SceneTarget->Width != width || m_SceneTarget->Height != height)
        {
            m_VulkanState->Device.waitIdle();
            m_SceneTarget.reset();
            m_SceneTarget = std::make_unique<RenderTarget>();
            CreateRenderTarget(*m_SceneTarget, width, height);
        }
    }

    nvrhi::TextureHandle Renderer::GetViewportTexture() const
    {
        return m_SceneTarget ? m_SceneTarget->Output : nullptr;
    }

    std::pair<uint32_t, uint32_t> Renderer::GetViewportSize() const
    {
        if (m_SceneTarget)
            return { m_SceneTarget->Width, m_SceneTarget->Height };

        return { m_VulkanState->SwapchainExtent.width, m_VulkanState->SwapchainExtent.height };
    }
}
