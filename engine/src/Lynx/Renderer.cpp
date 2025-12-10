#include "lxpch.h"
#include "Renderer.h"
#include <shaderc/shaderc.hpp>

namespace Lynx
{
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
    
    VulkanContext::VulkanContext(GLFWwindow* window)
        : Window(window)
    {
        // TODO: Enable validation extensions
        // 1. Instance
        uint32_t count;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        instanceCreateInfo.enabledExtensionCount = count;
        instanceCreateInfo.ppEnabledExtensionNames = extensions;
        vkCreateInstance(&instanceCreateInfo, nullptr, &Instance);

        // 2. Surface
        glfwCreateWindowSurface(Instance, Window, nullptr, &Surface);

        // 3. Physical Device
        uint32_t devCount = 1;
        vkEnumeratePhysicalDevices(Instance, &devCount, &PhysicalDevice);

        // 4. Queue Family
        GraphicsQueueFamily = 0; // Simplified: assuming index 0 has graphics

        // 5. Logical Device
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueCreateInfo.queueFamilyIndex = GraphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &priority;

        const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo devInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        devInfo.queueCreateInfoCount = 1;
        devInfo.pQueueCreateInfos = &queueCreateInfo;
        devInfo.enabledExtensionCount = 1;
        devInfo.ppEnabledExtensionNames = devExts;

        vkCreateDevice(PhysicalDevice, &devInfo, nullptr, &Device);
        vkGetDeviceQueue(Device, GraphicsQueueFamily, 0, &GraphicsQueue);

        // 6. Swapchain
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &caps);
        SwapchainExtent = caps.currentExtent;
        SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM; // Forced simplified format

        VkSwapchainCreateInfoKHR swapInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        swapInfo.surface = Surface;
        swapInfo.minImageCount = 2;
        swapInfo.imageFormat = SwapchainFormat;
        swapInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapInfo.imageExtent = SwapchainExtent;
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapInfo.preTransform = caps.currentTransform;
        swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapInfo.clipped = VK_TRUE;

        vkCreateSwapchainKHR(Device, &swapInfo, nullptr, &Swapchain);

        uint32_t imgCount;
        vkGetSwapchainImagesKHR(Device, Swapchain, &imgCount, nullptr);
        SwapchainImages.resize(imgCount);
        vkGetSwapchainImagesKHR(Device, Swapchain, &imgCount, SwapchainImages.data());
    }

    VulkanContext::~VulkanContext()
    {
        vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        vkDestroyDevice(Device, nullptr);
        vkDestroySurfaceKHR(Instance, Surface, nullptr);
        vkDestroyInstance(Instance, nullptr);
    }

    void Renderer::Init(VulkanContext& ctx)
    {
        // 1. Initialize NVRHI on top of Vulkan
        nvrhi::vulkan::DeviceDesc deviceDesc;
        deviceDesc.instance = ctx.Instance;
        deviceDesc.physicalDevice = ctx.PhysicalDevice;
        deviceDesc.device = ctx.Device;
        deviceDesc.graphicsQueue = ctx.GraphicsQueue;
        deviceDesc.graphicsQueueIndex = ctx.GraphicsQueueFamily;

        // TODO: Pass validation extensions

        m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

        // 2. Create NVRHI wrappers for Swapchain Images
        for (auto vkImage : ctx.SwapchainImages)
        {
            nvrhi::TextureDesc desc;
            desc.width = ctx.SwapchainExtent.width;
            desc.height = ctx.SwapchainExtent.height;
            desc.format = nvrhi::Format::BGRA8_UNORM; // Match Vulkan swapchain format!
            desc.isRenderTarget = true;
            desc.initialState = nvrhi::ResourceStates::Present; // Crucial: Vulkan swapchain starts in Present state
            desc.keepInitialState = true;

            // Create Handle
            nvrhi::TextureHandle texture = m_NvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, static_cast<void*>(vkImage), desc);
            // Create Framebuffer (Render Target) for this texture
            nvrhi::FramebufferDesc fbDesc;
            fbDesc.addColorAttachment(texture);
            m_Framebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
        }

        // 3. Compile Shaders
        // Note: We use a full screen triangle hack (gl_VertexIndex) so we don't need vertex buffers yet.
        const char* vsSrc = R"(
            #version 450
            void main() {
                const vec2 positions[3] = vec2[3](
                    vec2(0.0, -0.5),
                    vec2(0.5, 0.5),
                    vec2(-0.5, 0.5)
                );
                gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
            }
        )";

        const char* fsSrc = R"(
            #version 450
            layout(location = 0) out vec4 outColor;
            void main() {
                outColor = vec4(1.0, 0.5, 0.2, 1.0); // Orange
            }
        )";

        auto vsSpirv = CompileGLSL(vsSrc, shaderc_glsl_vertex_shader, "triangle.vert");
        auto fsSpirv = CompileGLSL(fsSrc, shaderc_glsl_fragment_shader, "triangle.frag");

        m_VertexShader = m_NvrhiDevice->createShader(nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex), vsSpirv.data(), vsSpirv.size() * 4);
        m_FragmentShader = m_NvrhiDevice->createShader(nvrhi::ShaderDesc(nvrhi::ShaderType::Pixel), fsSpirv.data(), fsSpirv.size() * 4);

        // 4. Create Pipeline
        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;
        pipeDesc.VS = m_VertexShader;
        pipeDesc.PS = m_FragmentShader;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

        // Use the first framebuffer to create the pipeline layout (compatible with all others)
        m_Pipeline = m_NvrhiDevice->createGraphicsPipeline(pipeDesc, m_Framebuffers[0]->getFramebufferInfo());

        // 5. Create Command List
        m_CommandList = m_NvrhiDevice->createCommandList();
    }

    void Renderer::Render(uint32_t swapChainIndex)
    {
        m_CommandList->open();

        // NVRHI handles the layout transitions (Present -> RenderTarget) automatically here
        m_CommandList->clearTextureFloat(m_Framebuffers[swapChainIndex]->getDesc().colorAttachments[0].texture,
            nvrhi::AllSubresources,
            nvrhi::Color(0.0f, 0.0f, 0.0f, 1.0f));

        nvrhi::GraphicsState state;
        state.pipeline = m_Pipeline;
        state.framebuffer = m_Framebuffers[swapChainIndex];

        // define viewport dynamically
        state.viewport.addViewport(nvrhi::Viewport(
            (float)m_Framebuffers[swapChainIndex]->getFramebufferInfo().width,
            (float)m_Framebuffers[swapChainIndex]->getFramebufferInfo().height
        ));

        m_CommandList->setGraphicsState(state);

        nvrhi::DrawArguments args;
        args.vertexCount = 3;
        m_CommandList->draw(args);

        m_CommandList->close();

        // Transition back to Present is handled by NVRHI automatically on close/execute 
        // because we set keepInitialState=true and initialState=Present in TextureDesc.
        m_NvrhiDevice->executeCommandList(m_CommandList);
    }
}
