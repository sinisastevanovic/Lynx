#include "lxpch.h"
#include "Renderer.h"
#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <shaderc/shaderc.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "nvrhi/utils.h"

namespace Lynx
{
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

        vk::Semaphore ImageAvailableSemaphore;
        vk::Semaphore RenderFinishedSemaphore;
    };
    
    std::vector<uint32_t> CompileGLSL(const std::string& source, shaderc_shader_kind kind, const char* fileName)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, fileName, options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            LX_CORE_ERROR("Shader Error: {0}", module.GetErrorMessage());
        }

        return {module.cbegin(), module.cend()};
    }

    Renderer::Renderer()
    {
        m_VulkanState = std::make_unique<VulkanState>();
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    void Renderer::Init(GLFWwindow* window)
    {
        InitVulkan(window);
        InitNVRHI();
        InitPipeline();

        LX_CORE_INFO("Renderer initialized successfully");
    }

    void Renderer::InitVulkan(GLFWwindow* window)
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(NULL, "vkGetInstanceProcAddr"));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        
        // TODO: Enable validation extensions
        // 1. Instance
        uint32_t count;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);

        vk::InstanceCreateInfo instInfo;
        instInfo.enabledExtensionCount = count;
        instInfo.ppEnabledExtensionNames = extensions;
        m_VulkanState->Instance = vk::createInstance(instInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanState->Instance);

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
        float priority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo;
        queueInfo.queueFamilyIndex = m_VulkanState->GraphicsQueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        vk::DeviceCreateInfo devInfo;
        devInfo.queueCreateInfoCount = 1;
        devInfo.pQueueCreateInfos = &queueInfo;
        devInfo.enabledExtensionCount = 1;
        devInfo.ppEnabledExtensionNames = devExts;
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
        vk::SemaphoreCreateInfo semInfo;
        m_VulkanState->ImageAvailableSemaphore = m_VulkanState->Device.createSemaphore(semInfo);
        m_VulkanState->RenderFinishedSemaphore = m_VulkanState->Device.createSemaphore(semInfo);
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

        // 2. Create NVRHI wrappers for Swapchain Images
        for (auto vkImage : m_VulkanState->SwapchainImages)
        {
            nvrhi::TextureDesc desc;
            desc.width = m_VulkanState->SwapchainExtent.width;
            desc.height = m_VulkanState->SwapchainExtent.height;
            desc.format = nvrhi::Format::BGRA8_UNORM; // Match Vulkan swapchain format!
            desc.isRenderTarget = true;
            desc.initialState = nvrhi::ResourceStates::Present; // Crucial: Vulkan swapchain starts in Present state
            desc.keepInitialState = true;

            // Create Handle
            nvrhi::TextureHandle texture = m_NvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, (VkImage)vkImage, desc);
            // Create Framebuffer (Render Target) for this texture
            nvrhi::FramebufferDesc fbDesc;
            fbDesc.addColorAttachment(texture);
            m_Framebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
        }

        // 3. Create Command List
        m_CommandList = m_NvrhiDevice->createCommandList();
    }

    void Renderer::InitPipeline()
    {
        // Hardcoded triangle shader
        const char* vsSrc = R"(
            #version 450
            void main() {
                const vec2 positions[3] = vec2[3]( vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5) );
                gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
            }
        )";

        const char* fsSrc = R"(
            #version 450
            layout(location = 0) out vec4 outColor;
            void main() { outColor = vec4(0.2, 0.8, 0.2, 1.0); } // Green
        )";

        auto vsSpirv = CompileGLSL(vsSrc, shaderc_glsl_vertex_shader, "triangle.vert");
        auto fsSpirv = CompileGLSL(fsSrc, shaderc_glsl_fragment_shader, "triangle.frag");

        m_VertexShader = m_NvrhiDevice->createShader(nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex), vsSpirv.data(), vsSpirv.size() * 4);
        m_FragmentShader = m_NvrhiDevice->createShader(nvrhi::ShaderDesc(nvrhi::ShaderType::Pixel), fsSpirv.data(), fsSpirv.size() * 4);

        // 2. Create Pipeline
        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;
        pipeDesc.VS = m_VertexShader;
        pipeDesc.PS = m_FragmentShader;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

        // Use the first framebuffer to create the pipeline layout (compatible with all others)
        m_Pipeline = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, m_Framebuffers[0]->getFramebufferInfo());
    }

    void Renderer::BeginFrame()
    {
        // 1. Acquire Image from Vulkan
        // TODO: handle resizing (VK_ERROR_OUT_OF_DATE_KHR)
        auto result = m_VulkanState->Device.acquireNextImageKHR(
            m_VulkanState->Swapchain,
            UINT64_MAX,
            vk::Semaphore(m_VulkanState->ImageAvailableSemaphore),
            nullptr
        );

        m_CurrentImageIndex = result.value;

        // 2. Start recording
        m_CommandList->open();

        // 3. Clear Screen
        nvrhi::utils::ClearColorAttachment(m_CommandList, m_Framebuffers[m_CurrentImageIndex], 0, nvrhi::Color(0.1f, 0.1f, 0.1f, 1.0f));

        // 4. Setup Draw State
        nvrhi::GraphicsState state;
        state.pipeline = m_Pipeline;
        state.framebuffer = m_Framebuffers[m_CurrentImageIndex];
        state.viewport.addViewport(nvrhi::Viewport(
            (float)m_Framebuffers[m_CurrentImageIndex]->getFramebufferInfo().width,
            (float)m_Framebuffers[m_CurrentImageIndex]->getFramebufferInfo().height
        ));

        m_CommandList->setGraphicsState(state);

        // 5. Draw
        nvrhi::DrawArguments args;
        args.vertexCount = 3;
        m_CommandList->draw(args);
    }

    void Renderer::EndFrame()
    {
        // 1. Close recording
        m_CommandList->close();

        // 2. Execute
        // We do not have complex semaphores setup for NVRHI internal submit yet, 
        // so we execute immediately.
        m_NvrhiDevice->executeCommandList(m_CommandList);

        m_NvrhiDevice->runGarbageCollection();

        m_VulkanState->Device.waitIdle();

        // 3. Present to Screen
        vk::PresentInfoKHR presentInfo;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_VulkanState->Swapchain;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

        // Note: We are cheating slightly on synchronization here to keep it "Simple".
        // In production, we need to pass a Semaphore to executeCommandList to signal, 
        // and wait on that semaphore here.
        // Instead, we force a wait for safety.
        //vkDeviceWaitIdle(m_VulkanState->Device);
        m_VulkanState->GraphicsQueue.presentKHR(&presentInfo);
    }

    void Renderer::Shutdown()
    {
        if (!m_VulkanState->Device)
            return;

        m_VulkanState->Device.waitIdle();

        m_Pipeline = nullptr;
        m_VertexShader = nullptr;
        m_FragmentShader = nullptr;
        m_CommandList = nullptr;
        m_Framebuffers.clear();
        m_NvrhiDevice = nullptr;

        m_VulkanState->Device.destroySemaphore(m_VulkanState->ImageAvailableSemaphore);
        m_VulkanState->Device.destroySemaphore(m_VulkanState->RenderFinishedSemaphore);
        m_VulkanState->Device.destroySwapchainKHR(m_VulkanState->Swapchain);
        m_VulkanState->Device.destroy();
        m_VulkanState->Instance.destroySurfaceKHR(m_VulkanState->Surface);
        m_VulkanState->Instance.destroy();
    }
}
