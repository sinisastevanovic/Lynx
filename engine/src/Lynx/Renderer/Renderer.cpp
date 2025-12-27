#include "Renderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "ShaderUtils.h"
#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

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

    struct PushData
    {
        glm::mat4 Model;
        glm::vec4 Color;
        float AlphaCutoff;
        int EntityID;
        float Padding[2];
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

        m_StageBuffer = nullptr;

        m_SamplerCache.clear();
        m_BindingLayout = nullptr;
        m_DefaultTexture = nullptr;
        m_DefaultNormalTexture = nullptr;
        m_DefaultBlackTexture = nullptr;
        m_DefaultMetallicRoughnessTexture = nullptr;
        m_BindingSet = nullptr;
        m_FrameBindingSets.clear();
        m_ConstantBuffer = nullptr;
        m_SwapchainDepth = nullptr;
        m_OpaqueQueue.clear();
        m_PipelineOpaque = nullptr;
        m_TransparentQueue.clear();
        m_PipelineTransparent = nullptr;
        m_VertexShader = nullptr;
        m_PixelShader = nullptr;
        m_CommandList = nullptr;
        m_SwapchainFramebuffers.clear();
        m_CurrentFramebuffer = nullptr;
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
        InitPipeline();
        InitShadows();
        LX_CORE_INFO("Renderer initialized successfully (Pipeline loaded).");
    }

    void Renderer::ReloadShaders()
    {
        m_VulkanState->Device.waitIdle();

        m_PipelineOpaque = nullptr;
        m_PipelineTransparent = nullptr;
        m_BindingLayout = nullptr;
        m_BindingSet = nullptr;
        m_VertexShader = nullptr;
        m_PixelShader = nullptr;

        InitPipeline();

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

    void Renderer::SetTexture(std::shared_ptr<Texture> texture)
    {
        nvrhi::TextureHandle handle = m_DefaultTexture;
        SamplerSettings samplerSettings;
        if (texture && texture->GetTextureHandle())
        {
            handle = texture->GetTextureHandle();
            samplerSettings = texture->GetSamplerSettings();
        }

        auto bindingSetDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, handle))
            .addItem(nvrhi::BindingSetItem::Sampler(2, GetSampler(samplerSettings)));

        m_BindingSet = m_NvrhiDevice->createBindingSet(bindingSetDesc, m_BindingLayout);
        m_FrameBindingSets.push_back(m_BindingSet);
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
        m_DefaultTexture = m_NvrhiDevice->createTexture(defaultTexDesc);

        nvrhi::TextureDesc defaultNormalTexDesc;
        defaultNormalTexDesc.width = 1;
        defaultNormalTexDesc.height = 1;
        defaultNormalTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultNormalTexDesc.debugName = "DefaultNormal";
        defaultNormalTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultNormalTexDesc.keepInitialState = true;
        m_DefaultNormalTexture = m_NvrhiDevice->createTexture(defaultNormalTexDesc);

        nvrhi::TextureDesc defaultMrTexDesc;
        defaultMrTexDesc.width = 1;
        defaultMrTexDesc.height = 1;
        defaultMrTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultMrTexDesc.debugName = "DefaultMetallicRoughness";
        defaultMrTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultMrTexDesc.keepInitialState = true;
        m_DefaultMetallicRoughnessTexture = m_NvrhiDevice->createTexture(defaultMrTexDesc);
        
        nvrhi::TextureDesc defaultBlackTexDesc;
        defaultBlackTexDesc.width = 1;
        defaultBlackTexDesc.height = 1;
        defaultBlackTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultBlackTexDesc.debugName = "DefaultBlack";
        defaultBlackTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultBlackTexDesc.keepInitialState = true;
        m_DefaultBlackTexture = m_NvrhiDevice->createTexture(defaultBlackTexDesc);

        uint32_t whitePixel = 0xFFFFFFFF;
        uint32_t normalPixel = 0xFFFF8080;
        uint32_t mrPixel = 0xFF8080FF;
        uint32_t blackPixel = 0xFF000000;
        m_CommandList->open();
        m_CommandList->writeTexture(m_DefaultTexture, 0, 0, &whitePixel, 4);
        m_CommandList->writeTexture(m_DefaultNormalTexture, 0, 0, &normalPixel, 4);
        m_CommandList->writeTexture(m_DefaultBlackTexture, 0, 0, &blackPixel, 4);
        m_CommandList->writeTexture(m_DefaultMetallicRoughnessTexture, 0, 0, &mrPixel, 4);
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
        m_ConstantBuffer = m_NvrhiDevice->createBuffer(cbDesc);

        if (!m_ConstantBuffer)
        {
            LX_CORE_ERROR("Failed to create Constant Buffer!");
        }
    }

    void Renderer::InitShadows()
    {
        // 1. Texture & Framebuffer (Depth Only)
        nvrhi::TextureDesc shadowMapDesc;
        shadowMapDesc.width = m_ShadowPass.Resolution;
        shadowMapDesc.height = m_ShadowPass.Resolution;
        shadowMapDesc.format = nvrhi::Format::D32;
        shadowMapDesc.isRenderTarget = true;
        shadowMapDesc.debugName = "ShadowMap";
        shadowMapDesc.initialState = nvrhi::ResourceStates::DepthWrite;
        shadowMapDesc.keepInitialState = true;
        m_ShadowPass.ShadowMap = m_NvrhiDevice->createTexture(shadowMapDesc);

        nvrhi::FramebufferDesc fbDesc;
        fbDesc.setDepthAttachment(m_ShadowPass.ShadowMap);
        m_ShadowPass.Framebuffer = m_NvrhiDevice->createFramebuffer(fbDesc);

        // 2. Constant Buffer (Shadow UBO)
        nvrhi::BufferDesc cbDesc;
        cbDesc.byteSize = sizeof(ShadowSceneData); // Just the mat4
        cbDesc.isConstantBuffer = true;
        cbDesc.debugName = "ShadowCB";
        cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        cbDesc.keepInitialState = true;
        m_ShadowPass.ConstantBuffer = m_NvrhiDevice->createBuffer(cbDesc);

        // 3. Pipeline & Layout
        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Shadow.glsl");

        // Layout: UBO (0) + Albedo(1) + Sampler(2)
        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.setVisibility(nvrhi::ShaderType::All);
        layoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0)); // Binding 0: UBO
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));    // Binding 1: Albedo
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(2));        // Binding 2: Sampler
        layoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(PushData)));
        layoutDesc.bindingOffsets.shaderResource = 0;
        layoutDesc.bindingOffsets.sampler = 0;
        layoutDesc.bindingOffsets.constantBuffer = 0;
        layoutDesc.bindingOffsets.unorderedAccess = 0;
        m_ShadowPass.BindingLayout = m_NvrhiDevice->createBindingLayout(layoutDesc);

        // Create a base BindingSet that just has the UBO
        // (We will need to create dynamic BindingSets for materials,
        //  OR we cheat and use one global binding set if we just bind white texture for everything opaque)
        // For proper masked support, we need per-material binding sets.
        // Let's create a "Default" binding set for Opaque objects that uses the White texture.

        nvrhi::BindingSetDesc bsDesc;
        bsDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ShadowPass.ConstantBuffer));
        bsDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_DefaultTexture)); // Default White
        bsDesc.addItem(nvrhi::BindingSetItem::Sampler(2, m_SamplerCache.begin()->second)); // Any linear sampler
        m_ShadowPass.BindingSet = m_NvrhiDevice->createBindingSet(bsDesc, m_ShadowPass.BindingLayout);

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Position))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("NORMAL")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Normal))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TANGENT")
                .setFormat(nvrhi::Format::RGBA32_FLOAT) // Note: RGBA for Tangent
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Tangent))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, TexCoord))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Color))
                .setElementStride(sizeof(Vertex))
        };

        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.bindingLayouts = { m_ShadowPass.BindingLayout };
        pipeDesc.VS = shader->GetVertexShader();
        pipeDesc.PS = shader->GetPixelShader();
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;
        pipeDesc.inputLayout = m_NvrhiDevice->createInputLayout(attributes, 5, shader->GetVertexShader());

        // Shadow Bias Settings
        pipeDesc.renderState.rasterState.depthBias = 1.0f;
        pipeDesc.renderState.rasterState.slopeScaledDepthBias = 2.0f;
        pipeDesc.renderState.rasterState.frontCounterClockwise = true;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;

        pipeDesc.renderState.depthStencilState.depthTestEnable = true;
        pipeDesc.renderState.depthStencilState.depthWriteEnable = true;
        pipeDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Less;

        m_ShadowPass.Pipeline = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, m_ShadowPass.Framebuffer->getFramebufferInfo());

        // 4. Create Comparison Sampler for Main Pass
        nvrhi::SamplerDesc shadowSamplerDesc;
        shadowSamplerDesc.addressU = shadowSamplerDesc.addressV = nvrhi::SamplerAddressMode::Border;
        shadowSamplerDesc.borderColor = nvrhi::Color(1.0f); // Shadows outside are white (lit)
        shadowSamplerDesc.reductionType = nvrhi::SamplerReductionType::Comparison;
        m_ShadowPass.ShadowSampler = m_NvrhiDevice->createSampler(shadowSamplerDesc);

    }

    void Renderer::RenderShadows()
    {
        m_CommandList->beginMarker("ShadowPass");

        nvrhi::utils::ClearDepthStencilAttachment(m_CommandList, m_ShadowPass.Framebuffer, 1.0f, 0);

        ShadowSceneData shadowData;
        shadowData.ViewProjectionMatrix = m_LightViewProjMatrix;
        m_CommandList->writeBuffer(m_ShadowPass.ConstantBuffer, &shadowData, sizeof(ShadowSceneData));

        // 2. Setup Render State
        nvrhi::GraphicsState state;
        state.pipeline = m_ShadowPass.Pipeline;
        state.framebuffer = m_ShadowPass.Framebuffer;
        state.viewport.addViewport(nvrhi::Viewport(m_ShadowPass.Resolution, m_ShadowPass.Resolution));
        state.viewport.addScissorRect(nvrhi::Rect(0, m_ShadowPass.Resolution, 0, m_ShadowPass.Resolution));

        // Bind the Default Set (UBO + White Texture) initially
        state.addBindingSet(m_ShadowPass.BindingSet);
        m_CommandList->setGraphicsState(state);

        // 3. Draw
        // Note: Ideally, for masked objects, you'd create a temporary BindingSet pointing to their texture.
        // For now, to keep this step focused, we will render everything as Opaque using the default white texture.
        // This means leaves will cast square shadows. We can fix "Masked" support in the next specific step if you want.

        auto drawQueue = [&](std::vector<RenderCommand>& queue) {
            for (const auto& cmd : queue) {
                const auto& submesh = cmd.Mesh->GetSubmeshes()[cmd.SubmeshIndex];

                PushData push;
                push.Model = cmd.Transform;
                push.AlphaCutoff = -1.0f; // Force opaque for now

                m_CommandList->setPushConstants(&push, sizeof(PushData));

                auto drawState = state;
                drawState.addVertexBuffer(nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0));
                drawState.setIndexBuffer(nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT));
                m_CommandList->setGraphicsState(drawState);
                
                nvrhi::DrawArguments args;
                args.vertexCount = submesh.IndexCount;

                m_CommandList->drawIndexed(args);
            }
        };

        drawQueue(m_OpaqueQueue);
        drawQueue(m_TransparentQueue);

        m_CommandList->endMarker();
    }

    void Renderer::Flush()
    {
        const auto& fbInfo = m_CurrentFramebuffer->getFramebufferInfo();
        nvrhi::Viewport viewport(fbInfo.width, fbInfo.height);
        nvrhi::Rect scissor(0, fbInfo.width, 0, fbInfo.height);
        
        std::sort(m_OpaqueQueue.begin(), m_OpaqueQueue.end(),
            [](const RenderCommand& a, const RenderCommand& b) { return a.DistanceToCamera < b.DistanceToCamera; });

        for (const auto& cmd : m_OpaqueQueue)
        {
            const auto& submesh = cmd.Mesh->GetSubmeshes()[cmd.SubmeshIndex];

            auto state = nvrhi::GraphicsState()
               .setPipeline(m_PipelineOpaque)
               .setFramebuffer(m_CurrentFramebuffer)
               .addVertexBuffer(nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0))
               .setIndexBuffer(nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT));
            state.viewport.addViewport(viewport);
            state.viewport.addScissorRect(scissor);

            auto bindingSetDesc = nvrhi::BindingSetDesc();
            BindMaterial(submesh.Material.get(), bindingSetDesc);
            auto bindingSet = m_NvrhiDevice->createBindingSet(bindingSetDesc, m_BindingLayout);
            state.addBindingSet(bindingSet);

            m_CommandList->setGraphicsState(state);

            PushData push;
            push.Model = cmd.Transform;
            push.Color = cmd.Color;
            push.EntityID = cmd.EntityID;
            push.AlphaCutoff = submesh.Material->Mode == AlphaMode::Mask ? submesh.Material->AlphaCutoff : -1.0f;
            m_CommandList->setPushConstants(&push, sizeof(PushData));
        
            nvrhi::DrawArguments args;
            args.vertexCount = submesh.IndexCount;
            m_CommandList->drawIndexed(args);
        }

        std::sort(m_TransparentQueue.begin(), m_TransparentQueue.end(),
            [](const RenderCommand& a, const RenderCommand& b) { return a.DistanceToCamera < b.DistanceToCamera; });

        for (const auto& cmd : m_TransparentQueue)
        {
            const auto& submesh = cmd.Mesh->GetSubmeshes()[cmd.SubmeshIndex];

            auto state = nvrhi::GraphicsState()
               .setPipeline(m_PipelineTransparent)
               .setFramebuffer(m_CurrentFramebuffer)
               .addVertexBuffer(nvrhi::VertexBufferBinding(submesh.VertexBuffer, 0, 0))
               .setIndexBuffer(nvrhi::IndexBufferBinding(submesh.IndexBuffer, nvrhi::Format::R32_UINT));
            state.viewport.addViewport(viewport);
            state.viewport.addScissorRect(scissor);

            auto bindingSetDesc = nvrhi::BindingSetDesc();
            BindMaterial(submesh.Material.get(), bindingSetDesc);
            auto bindingSet = m_NvrhiDevice->createBindingSet(bindingSetDesc, m_BindingLayout);
            state.addBindingSet(bindingSet);

            m_CommandList->setGraphicsState(state);

            PushData push;
            push.Model = cmd.Transform;
            push.Color = cmd.Color;
            push.EntityID = cmd.EntityID;
            push.AlphaCutoff = -1.0f;
            m_CommandList->setPushConstants(&push, sizeof(PushData));
        
            nvrhi::DrawArguments args;
            args.vertexCount = submesh.IndexCount;
            m_CommandList->drawIndexed(args);
        }

        m_OpaqueQueue.clear();
        m_TransparentQueue.clear();
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

    void Renderer::BindMaterial(Material* material, nvrhi::BindingSetDesc& desc)
    {
        nvrhi::TextureHandle albedo = m_DefaultTexture;
        nvrhi::TextureHandle normal = m_DefaultNormalTexture;
        nvrhi::TextureHandle mr = m_DefaultTexture;
        nvrhi::TextureHandle emiss = m_DefaultBlackTexture;

        SamplerSettings samplerSettings;
        samplerSettings.FilterMode = TextureFilter::Linear;
        samplerSettings.WrapMode = TextureWrap::Repeat;

        if (material)
        {
            if (material->AlbedoTexture.IsValid())
            {
                auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(material->AlbedoTexture);
                if (texture)
                {
                    albedo = texture->GetTextureHandle();
                    // TODO: If we want per-texture samplers, we need more sampler slots or combined image samplers
                    samplerSettings = texture->GetSamplerSettings();
                }
            }

            if (material->NormalMap.IsValid())
            {
                auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(material->NormalMap);
                if (texture)
                    normal = texture->GetTextureHandle();
            }

            if (material->MetallicRoughnessTexture.IsValid())
            {
                auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(material->MetallicRoughnessTexture);
                if (texture)
                    mr = texture->GetTextureHandle();
            }

            if (material->EmissiveTexture.IsValid())
            {
                auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(material->EmissiveTexture);
                if (texture)
                    emiss = texture->GetTextureHandle();
            }
        }

        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, albedo))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(2, normal))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(3, mr))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(4, emiss))
            .addItem(nvrhi::BindingSetItem::Sampler(5, GetSampler(samplerSettings)))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(6, m_ShadowPass.ShadowMap))
            .addItem(nvrhi::BindingSetItem::Sampler(7, m_ShadowPass.ShadowSampler));
    }

    void Renderer::InitPipeline()
    {
        std::shared_ptr<Shader> shaderAsset;
        if (m_ShouldCreateIDTarget)
        {
            shaderAsset = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/StandardEditor.glsl");
        }
        else
        {
            shaderAsset = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Standard.glsl");
        }

        LX_ASSERT(shaderAsset, "Failed to load standard shader!");

        m_VertexShader = shaderAsset->GetVertexShader();
        m_PixelShader = shaderAsset->GetPixelShader();
        
        auto layoutDesc = nvrhi::BindingLayoutDesc()
        .setVisibility(nvrhi::ShaderType::All)
        .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
        .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(PushData)))
        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)) // Albedo
        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)) // Normal
        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(3)) // MetallicRoughness
        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(4)) // Emissive
        .addItem(nvrhi::BindingLayoutItem::Sampler(5))
        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(6)) // Shadow map
        .addItem(nvrhi::BindingLayoutItem::Sampler(7));
        
        layoutDesc.bindingOffsets.shaderResource = 0;
        layoutDesc.bindingOffsets.sampler = 0;
        layoutDesc.bindingOffsets.constantBuffer = 0;
        layoutDesc.bindingOffsets.unorderedAccess = 0;
        m_BindingLayout = m_NvrhiDevice->createBindingLayout(layoutDesc);

        if (!m_BindingLayout)
        {
            LX_CORE_ERROR("Failed to create Binding Layout!");
        }

        SamplerSettings defaultSamplerSettings;
        defaultSamplerSettings.FilterMode = TextureFilter::Linear;
        defaultSamplerSettings.WrapMode = TextureWrap::Repeat;

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_DefaultTexture));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, m_DefaultNormalTexture));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, m_DefaultMetallicRoughnessTexture));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, m_DefaultBlackTexture));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(5, GetSampler(defaultSamplerSettings)));
        m_BindingSet = m_NvrhiDevice->createBindingSet(bindingSetDesc, m_BindingLayout);
        
        if (!m_BindingSet)
        {
            LX_CORE_ERROR("Failed to create Binding Set!");
        }
        
        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.bindingLayouts = { m_BindingLayout };
        pipeDesc.VS = m_VertexShader;
        pipeDesc.PS = m_PixelShader;
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;
        pipeDesc.renderState.rasterState.frontCounterClockwise = true;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;

        // Opaque Pipeline
        pipeDesc.renderState.depthStencilState.depthTestEnable = true;
        pipeDesc.renderState.depthStencilState.depthWriteEnable = true;
        pipeDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Less;
        pipeDesc.renderState.blendState.targets[0].setBlendEnable(false);

        if (m_ShouldCreateIDTarget)
        {
            pipeDesc.renderState.blendState.targets[1]
            .setBlendEnable(false)
            .setColorWriteMask(nvrhi::ColorMask::All);
        }

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Position))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("NORMAL")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Normal))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TANGENT")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Tangent))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, TexCoord))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(Vertex, Color))
                .setElementStride(sizeof(Vertex))
        };
        pipeDesc.inputLayout = m_NvrhiDevice->createInputLayout(attributes, 5, m_VertexShader);

        if (m_ShouldCreateIDTarget)
        {
            auto fbInfo = nvrhi::FramebufferInfo()
                .addColorFormat(nvrhi::Format::BGRA8_UNORM)
                .addColorFormat(nvrhi::Format::R32_SINT)
                .setDepthFormat(nvrhi::Format::D32);
            
            m_PipelineOpaque = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, fbInfo);
        }
        else
        {
            m_PipelineOpaque = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, m_SwapchainFramebuffers[0]->getFramebufferInfo());
        }

        // Transparent Pipeline
        pipeDesc.renderState.depthStencilState.depthWriteEnable = false;
        pipeDesc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)
            .setDestBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha);

        if (m_ShouldCreateIDTarget)
        {
            auto fbInfo = nvrhi::FramebufferInfo()
                .addColorFormat(nvrhi::Format::BGRA8_UNORM)
                .addColorFormat(nvrhi::Format::R32_SINT)
                .setDepthFormat(nvrhi::Format::D32);
            
            m_PipelineTransparent = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, fbInfo);
        }
        else
        {
            m_PipelineTransparent = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, m_SwapchainFramebuffers[0]->getFramebufferInfo());
        }
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

        m_FrameBindingSets.clear();
        // 2. Start recording
        m_CommandList->open();
        
        m_CameraPosition = cameraPosition;

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

        m_LightViewProjMatrix = lightProj * lightView;

        // Shadow Map Stabilization
        // We need to snap the projection to the nearest texel in Light Space to avoid shimmering
        float shadowMapRes = (float)m_ShadowPass.Resolution;
        
        // Project a reference point (Origin) to Shadow Map NDC Space
        glm::vec4 shadowOrigin = m_LightViewProjMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
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
        m_LightViewProjMatrix[3][0] += dX;
        m_LightViewProjMatrix[3][1] += dY;
        
        SceneData sceneData;
        sceneData.ViewProjectionMatrix = viewProjection;
        sceneData.LightViewProjection = m_LightViewProjMatrix;
        sceneData.CameraPosition = glm::vec4(cameraPosition, 1.0f);
        sceneData.LightDirection = glm::vec4(lightDir, lightIntensity);
        sceneData.LightColor = glm::vec4(lightColor, 1.0f);
        m_CommandList->writeBuffer(m_ConstantBuffer, &sceneData, sizeof(SceneData));

        if (m_EditorTarget)
        {
            m_CurrentFramebuffer = m_EditorTarget->Framebuffer;
            m_CommandList->clearTextureUInt(m_EditorTarget->IdBuffer, nvrhi::AllSubresources, (uint32_t)-1);
            //nvrhi::utils::ClearColorAttachment(m_CommandList, m_CurrentFramebuffer, 1, nvrhi::Color(-1.0f, 0.0f, 0.0f, 0.0f));
        }
        else
        {
            m_CurrentFramebuffer = m_SwapchainFramebuffers[m_CurrentImageIndex];
        }

        // 3. Clear Screen
        nvrhi::utils::ClearColorAttachment(m_CommandList, m_CurrentFramebuffer, 0, nvrhi::Color(0.1f, 0.1f, 0.1f, 1.0f));
        nvrhi::utils::ClearDepthStencilAttachment(m_CommandList, m_CurrentFramebuffer, 1.0f, 0);
    }

    void Renderer::SubmitMesh(std::shared_ptr<StaticMesh> mesh, const glm::mat4& transform, const glm::vec4& color, int entityID)
    {
        if (!mesh)
            return;

        float dist = glm::distance(m_CameraPosition, glm::vec3(transform[3]));

        for (int i = 0; i < mesh->GetSubmeshes().size(); ++i)
        {
            const auto& submesh = mesh->GetSubmeshes()[i];
            RenderCommand cmd = { mesh, i, transform, color, entityID, dist };

            if (submesh.Material->Mode == AlphaMode::Translucent)
                m_TransparentQueue.push_back(cmd);
            else
                m_OpaqueQueue.push_back(cmd);
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
        RenderShadows();
        Flush();
        
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
        m_CurrentFramebuffer = nullptr;
        
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
