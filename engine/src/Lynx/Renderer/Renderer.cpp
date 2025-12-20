#include "lxpch.h"
#include "Renderer.h"
#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

#include "nvrhi/validation.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <shaderc/shaderc.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "nvrhi/utils.h"
#include "Lynx/Renderer/EditorCamera.h"


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

    Renderer::Renderer(GLFWwindow* window)
    {
        m_VulkanState = std::make_unique<VulkanState>();
        InitVulkan(window);
        InitNVRHI();
        InitBuffers();
        InitPipeline();

        LX_CORE_INFO("Renderer initialized successfully");
    }

    Renderer::~Renderer()
    {
        LX_CORE_INFO("Renderer shutting down...");
        
        if (!m_VulkanState->Device)
            return;

        m_VulkanState->Device.waitIdle();

        if (m_NvrhiDevice)
            m_NvrhiDevice->runGarbageCollection();

        m_Sampler = nullptr;
        m_BindingLayout = nullptr;
        m_DefaultTexture = nullptr;
        m_BindingSet = nullptr;
        m_ConstantBuffer = nullptr;
        m_CubeIndexBuffer = nullptr;
        m_CubeVertexBuffer = nullptr;
        m_DepthBuffer = nullptr;
        m_Pipeline = nullptr;
        m_VertexShader = nullptr;
        m_FragmentShader = nullptr;
        m_CommandList = nullptr;
        m_Framebuffers.clear();
        m_NvrhiDevice = nullptr;

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

    void Renderer::SetTexture(std::shared_ptr<Texture> texture)
    {
        nvrhi::TextureHandle handle = m_DefaultTexture;
        if (texture && texture->GetTextureHandle())
        {
            handle = texture->GetTextureHandle();
        }

        auto bindingSetDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, handle))
            .addItem(nvrhi::BindingSetItem::Sampler(2, m_Sampler));

        m_BindingSet = m_NvrhiDevice->createBindingSet(bindingSetDesc, m_BindingLayout);
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
        m_DepthBuffer = m_NvrhiDevice->createTexture(depthDesc);

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
            fbDesc.setDepthAttachment(m_DepthBuffer);
            m_Framebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
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

        uint32_t whitePixel = 0xFFFFFFFF;
        m_CommandList->open();
        m_CommandList->writeTexture(m_DefaultTexture, 0, 0, &whitePixel, 4);
        m_CommandList->close();
        m_NvrhiDevice->executeCommandList(m_CommandList);
    }
    
    struct Vertex
    {
        float x, y, z;
        float u, v;
    };
    
    void Renderer::InitBuffers()
    {
        const Vertex cubeVertices[] = {
            // Front Face
            {-0.5f, -0.5f,  0.5f,  0.0f, 1.0f},
            { 0.5f, -0.5f,  0.5f,  1.0f, 1.0f},
            { 0.5f,  0.5f,  0.5f,  1.0f, 0.0f},
            {-0.5f,  0.5f,  0.5f,  0.0f, 0.0f},
            // Back Face
            {-0.5f, -0.5f, -0.5f,  1.0f, 1.0f},
            { 0.5f, -0.5f, -0.5f,  0.0f, 1.0f},
            { 0.5f,  0.5f, -0.5f,  0.0f, 0.0f},
            {-0.5f,  0.5f, -0.5f,  1.0f, 0.0f}
        };

        const uint32_t cubeIndices[] = {
            0, 1, 2, 2, 3, 0,
            1, 5, 6, 6, 2, 1,
            7, 6, 5, 5, 4, 7,
            4, 0, 3, 3, 7, 4,
            3, 2, 6, 6, 7, 3,
            4, 5, 1, 1, 0, 4
        };

        nvrhi::BufferDesc vbDesc;
        vbDesc.byteSize = sizeof(cubeVertices);
        vbDesc.isVertexBuffer = true;
        vbDesc.initialState = nvrhi::ResourceStates::CopyDest;
        vbDesc.keepInitialState = true; 
        m_CubeVertexBuffer = m_NvrhiDevice->createBuffer(vbDesc);

        nvrhi::BufferDesc ibDesc;
        ibDesc.byteSize = sizeof(cubeIndices);
        ibDesc.isIndexBuffer = true;
        ibDesc.initialState = nvrhi::ResourceStates::CopyDest;
        ibDesc.keepInitialState = true; 
        m_CubeIndexBuffer = m_NvrhiDevice->createBuffer(ibDesc);
        
        m_CommandList->open();
        m_CommandList->writeBuffer(m_CubeVertexBuffer, cubeVertices, sizeof(cubeVertices));
        m_CommandList->writeBuffer(m_CubeIndexBuffer, cubeIndices, sizeof(cubeIndices));
        m_CommandList->close();
        m_NvrhiDevice->executeCommandList(m_CommandList);

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

    void Renderer::InitPipeline()
    {
        const char* vsSrc = R"(
        #version 450
        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec2 a_TexCoord;

        layout(location = 0) out vec2 v_TexCoord;
            
        layout(set = 0, binding = 0) uniform UBO {
        mat4 u_ViewProjection;
        } ubo;

        layout(push_constant) uniform PushConsts {
        mat4 u_Model;
        } push;

        void main() {
        v_TexCoord = a_TexCoord;
        gl_Position = ubo.u_ViewProjection * push.u_Model * vec4(a_Position, 1.0);
        }
        )";
        /*const char* vsSrc = R"(
            #version 450
            layout(location = 0) in vec3 a_Position;

            
            layout(set = 0, binding = 0) uniform UBO {
                mat4 u_ViewProjection;
            } ubo;

            layout(push_constant) uniform PushConsts {
                mat4 u_Model;
            } push;

            void main() {
                gl_Position = ubo.u_ViewProjection * push.u_Model * vec4(a_Position, 1.0);
            }
        )";*/

        const char* fsSrc = R"(
        #version 450
        layout(location = 0) in vec2 v_TexCoord;
        layout(location = 0) out vec4 outColor;
            
        layout(set = 0, binding = 1) uniform texture2D u_Texture;
        layout(set = 0, binding = 2) uniform sampler u_Sampler;
    
        void main() { 
        outColor = texture(sampler2D(u_Texture, u_Sampler), v_TexCoord); 
        }
        )";

        /*const char* fsSrc = R"(
            #version 450
            layout(location = 0) out vec4 outColor;
            
    
            void main() { 
                outColor = outColor = vec4(0.2, 0.8, 0.2, 1.0);
            }
        )";*/

        auto vsSpirv = CompileGLSL(vsSrc, shaderc_glsl_vertex_shader, "cube.vert");
        auto fsSpirv = CompileGLSL(fsSrc, shaderc_glsl_fragment_shader, "cube.frag");

        m_VertexShader = m_NvrhiDevice->createShader(nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex), vsSpirv.data(), vsSpirv.size() * 4);
        m_FragmentShader = m_NvrhiDevice->createShader(nvrhi::ShaderDesc(nvrhi::ShaderType::Pixel), fsSpirv.data(), fsSpirv.size() * 4);

        auto samplerDesc = nvrhi::SamplerDesc()
            .setAddressU(nvrhi::SamplerAddressMode::Repeat)
            .setAddressV(nvrhi::SamplerAddressMode::Repeat)
            .setMinFilter(true)
            .setMagFilter(true);
        m_Sampler = m_NvrhiDevice->createSampler(samplerDesc);
        
        auto layoutDesc = nvrhi::BindingLayoutDesc()
        .setVisibility(nvrhi::ShaderType::All)
        .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
        .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(glm::mat4)))
        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1))
        .addItem(nvrhi::BindingLayoutItem::Sampler(2)); // Moved to Binding 2
        
        layoutDesc.bindingOffsets.shaderResource = 0;
        layoutDesc.bindingOffsets.sampler = 0;
        layoutDesc.bindingOffsets.constantBuffer = 0;
        layoutDesc.bindingOffsets.unorderedAccess = 0;
        m_BindingLayout = m_NvrhiDevice->createBindingLayout(layoutDesc);

        if (!m_BindingLayout)
        {
            LX_CORE_ERROR("Failed to create Binding Layout!");
        }

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_DefaultTexture));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(2, m_Sampler));
        m_BindingSet = m_NvrhiDevice->createBindingSet(bindingSetDesc, m_BindingLayout);
        
        if (!m_BindingSet)
        {
            LX_CORE_ERROR("Failed to create Binding Set!");
        }
        
        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.bindingLayouts = { m_BindingLayout };
        pipeDesc.VS = m_VertexShader;
        pipeDesc.PS = m_FragmentShader;
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipeDesc.renderState.depthStencilState.depthTestEnable = true;
        pipeDesc.renderState.depthStencilState.depthWriteEnable = true;
        pipeDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Less;
                
        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(0)
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setBufferIndex(0)
                .setOffset(sizeof(float) * 3)
                .setElementStride(sizeof(Vertex))
        };
        pipeDesc.inputLayout = m_NvrhiDevice->createInputLayout(attributes, 2, m_VertexShader);                
        m_Pipeline = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, m_Framebuffers[0]->getFramebufferInfo());
    }

    void Renderer::BeginScene(const EditorCamera& camera)
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
        
        SceneData sceneData;
        sceneData.ViewProjectionMatrix = camera.GetViewProjection();
        m_CommandList->writeBuffer(m_ConstantBuffer, &sceneData, sizeof(SceneData));

        // 3. Clear Screen
        nvrhi::utils::ClearColorAttachment(m_CommandList, m_Framebuffers[m_CurrentImageIndex], 0, nvrhi::Color(0.1f, 0.1f, 0.1f, 1.0f));
        nvrhi::utils::ClearDepthStencilAttachment(m_CommandList, m_Framebuffers[m_CurrentImageIndex], 1.0f, 0);

        // 4. Setup Draw State
        nvrhi::GraphicsState state;
        state.pipeline = m_Pipeline;
        state.framebuffer = m_Framebuffers[m_CurrentImageIndex];
        state.bindings = { m_BindingSet };
        state.addVertexBuffer(nvrhi::VertexBufferBinding(m_CubeVertexBuffer, 0, 0));
        state.indexBuffer = nvrhi::IndexBufferBinding(m_CubeIndexBuffer, nvrhi::Format::R32_UINT);

        nvrhi::Viewport viewport = nvrhi::Viewport(
            (float)m_Framebuffers[m_CurrentImageIndex]->getFramebufferInfo().width,
            (float)m_Framebuffers[m_CurrentImageIndex]->getFramebufferInfo().height
        );
        state.viewport.addViewport(viewport);
        state.viewport.addScissorRect(nvrhi::Rect(
            0, m_Framebuffers[m_CurrentImageIndex]->getFramebufferInfo().width,
            0, m_Framebuffers[m_CurrentImageIndex]->getFramebufferInfo().height
        ));

        m_CommandList->setGraphicsState(state);

        // 5. Draw
        //nvrhi::DrawArguments args;
        //args.vertexCount = 36;
        //m_CommandList->drawIndexed(args);
    }

    void Renderer::SubmitMesh(const glm::mat4& transform, const glm::vec4& color)
    {
        m_CommandList->setPushConstants(&transform, sizeof(glm::mat4));
        
        nvrhi::DrawArguments args;
        args.vertexCount = 36;
        m_CommandList->drawIndexed(args);
    }

    void Renderer::EndScene()
    {
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
    
    void Renderer::OnResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) return;

        m_VulkanState->Device.waitIdle();

        m_Framebuffers.clear();
        m_DepthBuffer = nullptr;
        
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
        m_DepthBuffer = m_NvrhiDevice->createTexture(depthDesc);

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
            fbDesc.setDepthAttachment(m_DepthBuffer);
            m_Framebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
        }
        
        // Reset current image index/frame?
        // m_CurrentImageIndex = 0; // Acquire will set this.
        // m_CurrentFrame = 0; // Sync frames are separate from swapchain images. Keep cycling.
    }
}
