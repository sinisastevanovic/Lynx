#include "Renderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "ShaderUtils.h"
#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

#include "ForwardPass.h"
#include "ShadowPass.h"
#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "nvrhi/validation.h"

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
                case TextureFormat::Depth32:
                    return nvrhi::Format::D32;
                case TextureFormat::Depth24Stencil8:
                    return nvrhi::Format::D24S8;
            }

            return nvrhi::Format::UNKNOWN;
        }

        static nvrhi::SamplerAddressMode WrapModeToNvrhi(TextureWrap wrap)
        {
            switch (wrap)
            {
                case TextureWrap::Repeat: return nvrhi::SamplerAddressMode::Repeat;
                case TextureWrap::Clamp: return nvrhi::SamplerAddressMode::Clamp;
                case TextureWrap::Mirror: return nvrhi::SamplerAddressMode::Mirror;
            }
            return nvrhi::SamplerAddressMode::Repeat;
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
    };
    
    Renderer::Renderer(GLFWwindow* window, bool initIdTarget)
        : m_ShouldCreateIDTarget(initIdTarget)
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

        m_Pipeline.Clear();
        m_RenderContext = RenderContext();
        m_CurrentFrameData = RenderData();
        m_StageBuffer = nullptr;

        m_SamplerCache.clear();
        m_WhiteTex = nullptr;
        m_NormalTex = nullptr;
        m_BlackTex = nullptr;
        m_MetallicRoughnessTex = nullptr;
        m_GlobalCB = nullptr;
        m_SwapchainDepth = nullptr;
        m_CommandList = nullptr;
        m_SwapchainFramebuffers.clear();
        if (m_EditorTarget)
        {
            m_EditorTarget->Depth = nullptr;
            m_EditorTarget->Color = nullptr;
            m_EditorTarget->IdBuffer = nullptr;
            m_EditorTarget->Framebuffer = nullptr;
            m_EditorTarget.reset();
        }
        m_NvrhiDevice = nullptr;
        m_ImGuiBackend.reset();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            m_VulkanState->Device.destroySemaphore(m_VulkanState->ImageAvailableSemaphores[i]);
            m_VulkanState->Device.destroySemaphore(m_VulkanState->RenderFinishedSemaphores[i]);
            m_VulkanState->Device.destroyFence(m_VulkanState->InFlightFences[i]);
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
        m_RenderContext.GetSampler = [this](const SamplerSettings& s) { return GetSampler(s); };

        // TODO: Make sure this is up-to-date...
        nvrhi::FramebufferInfo fbInfo;
        fbInfo.addColorFormat(nvrhi::Format::BGRA8_UNORM);
        if (m_ShouldCreateIDTarget) fbInfo.addColorFormat(nvrhi::Format::R32_SINT);
        fbInfo.setDepthFormat(nvrhi::Format::D32);
        m_RenderContext.PresentationFramebufferInfo = fbInfo;

        m_Pipeline.AddPass(std::make_unique<ShadowPass>(m_ShadowMapResolution));
        m_Pipeline.AddPass(std::make_unique<ForwardPass>());

        m_Pipeline.Init(m_RenderContext);
        
        LX_CORE_INFO("Renderer initialized successfully (Pipeline loaded).");
    }

    void Renderer::ReloadShaders()
    {
        m_VulkanState->Device.waitIdle();

        m_Pipeline.Init(m_RenderContext);
        LX_CORE_INFO("Shaders Reloaded and Pipeline Recreated!");
    }

    nvrhi::SamplerHandle Renderer::GetSampler(const SamplerSettings& settings)
    {
        auto it = m_SamplerCache.find(settings);
        if (it != m_SamplerCache.end())
            return it->second;

        // Create a new Sampler
        auto desc = nvrhi::SamplerDesc();

        nvrhi::SamplerAddressMode addressMode = Helpers::WrapModeToNvrhi(settings.WrapMode);
        desc.setAddressU(addressMode).setAddressV(addressMode).setAddressW(addressMode);

        if (settings.FilterMode == TextureFilter::Linear)
            desc.setMinFilter(false).setMagFilter(false).setMipFilter(false);
        else
            desc.setMinFilter(true).setMagFilter(true).setMipFilter(false); // TODO: mipFilter

        nvrhi::SamplerHandle handle = m_NvrhiDevice->createSampler(desc);
        m_SamplerCache[settings] = handle;
        return handle;
    }

    nvrhi::TextureHandle Renderer::CreateTexture(const TextureSpecification& specification, unsigned char* data)
    {
        auto desc = nvrhi::TextureDesc()
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setWidth(specification.Width)
            .setHeight(specification.Height)
            .setFormat(Helpers::TextureFormatToNvrhi(specification.Format, specification.IsSRGB))
            .setDebugName(specification.DebugName)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
            //.setMipLevels(specification.GenerateMips ? (uint32_t)(floor(log2(std::max(specification.Width, specification.Height))) + 1) : 1);
            //.setMipLevels(1);

        auto result = m_NvrhiDevice->createTexture(desc);
        if (!result)
        {
            LX_CORE_ERROR("Failed to create NVRHI texture from data");
            return nullptr;
        }

        if (data)
        {
            uint32_t bytesPerPixel = 4; // TODO: Get this from format

            size_t rowPitch = specification.Width * bytesPerPixel;
            size_t depthPitch = 0;

            // TODO: Batch this!
            // TODO: Generate mips!
            auto cmdList = m_NvrhiDevice->createCommandList();
            cmdList->open();
            cmdList->writeTexture(result, 0, 0, data, rowPitch, depthPitch);
            cmdList->close();
            m_NvrhiDevice->executeCommandList(cmdList);
        }
        
        return result;
    }

    bool Renderer::InitImGui()
    {
        m_ImGuiBackend = std::make_unique<ImGui_NVRHI>();
        return m_ImGuiBackend->init(m_NvrhiDevice);
    }

    void Renderer::RenderImGui()
    {
        if (m_ImGuiBackend)
        {
            m_ImGuiBackend->render(m_SwapchainFramebuffers[m_CurrentImageIndex]);
        }
    }

    int Renderer::ReadIdFromBuffer(uint32_t x, uint32_t y)
    {
        if (!m_EditorTarget || !m_EditorTarget->IdBuffer)
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

        m_CommandList->copyTexture(m_StageBuffer, dstSlice, m_EditorTarget->IdBuffer, srcSlice);
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

        vk::PhysicalDeviceFeatures features10;
        features10.geometryShader = VK_TRUE;
        features10.independentBlend = VK_TRUE;

        features13.pNext = &features12;
        
        float priority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo;
        queueInfo.queueFamilyIndex = m_VulkanState->GraphicsQueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        std::vector<const char*> deviceExtensions = { 
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
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
        
        // 6. Swapchain
        vk::SurfaceCapabilitiesKHR caps = m_VulkanState->PhysicalDevice.getSurfaceCapabilitiesKHR(m_VulkanState->Surface);
        m_VulkanState->SwapchainExtent = caps.currentExtent;
        m_VulkanState->SwapchainFormat = vk::Format::eB8G8R8A8Unorm; // Forced simplified format

        vk::SwapchainCreateInfoKHR swapInfo;
        swapInfo.surface = m_VulkanState->Surface;
        swapInfo.minImageCount = 2;
        swapInfo.imageFormat = m_VulkanState->SwapchainFormat;
        swapInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        swapInfo.imageExtent = m_VulkanState->SwapchainExtent;
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
        swapInfo.preTransform = caps.currentTransform;
        swapInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapInfo.presentMode = vk::PresentModeKHR::eFifo;
        swapInfo.clipped = VK_TRUE;
        m_VulkanState->Swapchain = m_VulkanState->Device.createSwapchainKHR(swapInfo);

        // 7. Get Images
        m_VulkanState->SwapchainImages = m_VulkanState->Device.getSwapchainImagesKHR(m_VulkanState->Swapchain);
        
        // 8. Sync Objects
        m_VulkanState->ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_VulkanState->RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_VulkanState->InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        vk::SemaphoreCreateInfo semInfo;
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            m_VulkanState->ImageAvailableSemaphores[i] = m_VulkanState->Device.createSemaphore(semInfo);
            m_VulkanState->RenderFinishedSemaphores[i] = m_VulkanState->Device.createSemaphore(semInfo);
            m_VulkanState->InFlightFences[i] = m_VulkanState->Device.createFence(fenceInfo);
        }
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

        // TODO: Pass validation extensions
        m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

        /*if (enableValidationLayers)
        {
            nvrhi::DeviceHandle nvrhiValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
            m_NvrhiDevice = nvrhiValidationLayer;
        }*/

        nvrhi::TextureDesc depthDesc;
        depthDesc.width = m_VulkanState->SwapchainExtent.width;
        depthDesc.height = m_VulkanState->SwapchainExtent.height;
        depthDesc.format = nvrhi::Format::D32;
        depthDesc.isRenderTarget = true;
        depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
        depthDesc.keepInitialState = true;
        m_SwapchainDepth = m_NvrhiDevice->createTexture(depthDesc);

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
            fbDesc.setDepthAttachment(m_SwapchainDepth);
            m_SwapchainFramebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
        }

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
            .setFormat(nvrhi::Format::BGRA8_UNORM)
            .setIsRenderTarget(true)
            .setDebugName("OffscreenColor")
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true);
        target.Color = m_NvrhiDevice->createTexture(colorDesc);

        auto depthDesc = nvrhi::TextureDesc()
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::D32)
            .setIsRenderTarget(true)
            .setDebugName("OffscreenDepth")
            .setInitialState(nvrhi::ResourceStates::DepthWrite)
            .setKeepInitialState(true);
        target.Depth = m_NvrhiDevice->createTexture(depthDesc);

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

        auto fbDesc = nvrhi::FramebufferDesc()
            .addColorAttachment(target.Color)
            .addColorAttachment(target.IdBuffer)
            .setDepthAttachment(target.Depth);
        target.Framebuffer = m_NvrhiDevice->createFramebuffer(fbDesc);
    }

    void Renderer::BeginScene(const glm::mat4& viewProjection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity)
    {
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

        // 2. Start recording
        m_CommandList->open();

        m_CurrentFrameData.OpaqueQueue.clear();
        m_CurrentFrameData.TransparentQueue.clear();
        
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

        m_CurrentFrameData.ViewProjection = viewProjection;
        m_CurrentFrameData.CameraPosition = cameraPosition;
        m_CurrentFrameData.LightDirection = lightDir;
        m_CurrentFrameData.LightColor = lightColor;
        m_CurrentFrameData.LightIntensity = lightIntensity;
        m_CurrentFrameData.LightViewProj = lightViewProj;
        if (m_EditorTarget)
        {
            m_CurrentFrameData.TargetFramebuffer = m_EditorTarget->Framebuffer;
            m_CommandList->clearTextureUInt(m_EditorTarget->IdBuffer, nvrhi::AllSubresources, (uint32_t)-1);
        }
        else
        {
            m_CurrentFrameData.TargetFramebuffer = m_SwapchainFramebuffers[m_CurrentFrame];
        }
        
        SceneData sceneData;
        sceneData.ViewProjectionMatrix = viewProjection;
        sceneData.LightViewProjection = lightViewProj;
        sceneData.CameraPosition = glm::vec4(cameraPosition, 1.0f);
        sceneData.LightDirection = glm::vec4(lightDir, lightIntensity);
        sceneData.LightColor = glm::vec4(lightColor, 1.0f);
        m_CommandList->writeBuffer(m_GlobalCB, &sceneData, sizeof(SceneData));

        // 3. Clear Screen
        nvrhi::utils::ClearColorAttachment(m_CommandList, m_CurrentFrameData.TargetFramebuffer, 0, nvrhi::Color(0.1f, 0.1f, 0.1f, 1.0f));
        nvrhi::utils::ClearDepthStencilAttachment(m_CommandList, m_CurrentFrameData.TargetFramebuffer, 1.0f, 0);
    }

    void Renderer::SubmitMesh(std::shared_ptr<StaticMesh> mesh, const glm::mat4& transform, const glm::vec4& color, int entityID)
    {
        if (!mesh)
            return;

        float dist = glm::distance(m_CurrentFrameData.CameraPosition, glm::vec3(transform[3]));

        for (int i = 0; i < mesh->GetSubmeshes().size(); ++i)
        {
            const auto& submesh = mesh->GetSubmeshes()[i];
            RenderCommand cmd = { mesh, i, transform, color, entityID, dist };

            if (submesh.Material->Mode == AlphaMode::Translucent)
                m_CurrentFrameData.TransparentQueue.push_back(cmd);
            else
                m_CurrentFrameData.OpaqueQueue.push_back(cmd);
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

    void Renderer::EndScene()
    {
        m_Pipeline.Execute(m_RenderContext, m_CurrentFrameData);
        
        // 1. Close recording
        m_CommandList->close();

        // 2. Execute
        nvrhi::vulkan::IDevice* vkDevice = static_cast<nvrhi::vulkan::IDevice*>(m_NvrhiDevice.Get());
        
        // SYNC: Wait for the image to be available before executing
        //vkDevice->waitForIdle();
        vkDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, (VkSemaphore)m_VulkanState->ImageAvailableSemaphores[m_CurrentFrame], 0);

        // SYNC: Signal RenderFinished after executing
        vkDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, (VkSemaphore)m_VulkanState->RenderFinishedSemaphores[m_CurrentFrame], 0);

        m_NvrhiDevice->executeCommandList(m_CommandList);

        if (m_EditorTarget)
        {
            m_CommandList->open();
            m_CommandList->setTextureState(m_EditorTarget->Color, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            m_CommandList->commitBarriers();
            nvrhi::utils::ClearColorAttachment(m_CommandList, m_SwapchainFramebuffers[m_CurrentImageIndex], 0, nvrhi::Color(0,0,0,1));
            m_CommandList->close();
            m_NvrhiDevice->executeCommandList(m_CommandList);
        }
        
        RenderImGui();

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
        presentInfo.pWaitSemaphores = &m_VulkanState->RenderFinishedSemaphores[m_CurrentFrame];

        m_VulkanState->GraphicsQueue.presentKHR(&presentInfo);
        
        // Heavy wait for debugging
        m_VulkanState->Device.waitIdle();
        
        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
        nvrhi::CommandListHandle commandList = m_NvrhiDevice->createCommandList();
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

        m_VulkanState->Device.waitIdle();

        m_SwapchainFramebuffers.clear();
        m_SwapchainDepth = nullptr;
        m_CurrentFrameData.TargetFramebuffer = nullptr;
        
        // We don't strictly need to clear the vector of vk::Image handles, 
        // as getSwapchainImagesKHR will overwrite it, but it's cleaner.
        m_VulkanState->SwapchainImages.clear();

        m_VulkanState->Device.destroySwapchainKHR(m_VulkanState->Swapchain);

        // Recreate Swapchain
        vk::SurfaceCapabilitiesKHR caps = m_VulkanState->PhysicalDevice.getSurfaceCapabilitiesKHR(m_VulkanState->Surface);
        m_VulkanState->SwapchainExtent = caps.currentExtent;
        // If extent is undefined (0xFFFFFFFF), use window size
        if (m_VulkanState->SwapchainExtent.width == UINT32_MAX)
        {
            m_VulkanState->SwapchainExtent.width = width;
            m_VulkanState->SwapchainExtent.height = height;
        }

        vk::SwapchainCreateInfoKHR swapInfo;
        swapInfo.surface = m_VulkanState->Surface;
        swapInfo.minImageCount = 2;
        swapInfo.imageFormat = m_VulkanState->SwapchainFormat;
        swapInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        swapInfo.imageExtent = m_VulkanState->SwapchainExtent;
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
        swapInfo.preTransform = caps.currentTransform;
        swapInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapInfo.presentMode = vk::PresentModeKHR::eFifo;
        swapInfo.clipped = VK_TRUE;
        
        m_VulkanState->Swapchain = m_VulkanState->Device.createSwapchainKHR(swapInfo);
        m_VulkanState->SwapchainImages = m_VulkanState->Device.getSwapchainImagesKHR(m_VulkanState->Swapchain);

        // Recreate NVRHI Resources
        nvrhi::TextureDesc depthDesc;
        depthDesc.width = m_VulkanState->SwapchainExtent.width;
        depthDesc.height = m_VulkanState->SwapchainExtent.height;
        depthDesc.format = nvrhi::Format::D32;
        depthDesc.isRenderTarget = true;
        depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
        depthDesc.keepInitialState = true;
        m_SwapchainDepth = m_NvrhiDevice->createTexture(depthDesc);

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
            fbDesc.setDepthAttachment(m_SwapchainDepth);
            m_SwapchainFramebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
        }
        
        // Reset current image index/frame?
        // m_CurrentImageIndex = 0; // Acquire will set this.
        // m_CurrentFrame = 0; // Sync frames are separate from swapchain images. Keep cycling.
    }

    void Renderer::EnsureEditorViewport(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        if (!m_EditorTarget)
        {
            m_EditorTarget = std::make_unique<RenderTarget>();
            CreateRenderTarget(*m_EditorTarget, width, height);
        }
        else if (m_EditorTarget->Width != width || m_EditorTarget->Height != height)
        {
            m_VulkanState->Device.waitIdle();
            m_EditorTarget->Framebuffer = nullptr;
            m_EditorTarget->Color = nullptr;
            m_EditorTarget->Depth = nullptr;
            CreateRenderTarget(*m_EditorTarget, width, height);
        }
    }

    nvrhi::TextureHandle Renderer::GetViewportTexture() const
    {
        return m_EditorTarget ? m_EditorTarget->Color : nullptr;
    }

    std::pair<uint32_t, uint32_t> Renderer::GetViewportSize() const
    {
        if (m_EditorTarget)
            return { m_EditorTarget->Width, m_EditorTarget->Height };

        return { m_VulkanState->SwapchainExtent.width, m_VulkanState->SwapchainExtent.height };
    }
}
