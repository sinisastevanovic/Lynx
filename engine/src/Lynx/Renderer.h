#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#undef GLFW_INCLUDED_VULKAN
#include <nvrhi/nvrhi.h>
#include <nvrhi/vulkan.h>

struct GLFWwindow;

namespace Lynx
{
    struct VulkanContext
    {
        GLFWwindow* Window;
        VkInstance Instance;
        VkSurfaceKHR Surface;
        VkPhysicalDevice PhysicalDevice;
        VkDevice Device;
        VkQueue GraphicsQueue;
        uint32_t GraphicsQueueFamily;
        VkSwapchainKHR Swapchain;
        VkFormat SwapchainFormat;
        VkExtent2D SwapchainExtent;
        std::vector<VkImage> SwapchainImages;

        VulkanContext(GLFWwindow* window);
        ~VulkanContext();
    };
    
    class Renderer
    {
    public:
        void Init(VulkanContext& ctx);
        void Render(uint32_t swapChainIndex);

    private:
        nvrhi::DeviceHandle m_NvrhiDevice;
        nvrhi::CommandListHandle m_CommandList;
        nvrhi::ShaderHandle m_VertexShader;
        nvrhi::ShaderHandle m_FragmentShader;
        nvrhi::GraphicsPipelineHandle m_Pipeline;

        // One NVRHI framebuffer wrapper per swapchain image
        std::vector<nvrhi::FramebufferHandle> m_Framebuffers;
    };
}

