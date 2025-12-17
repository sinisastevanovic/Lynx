#pragma once

#include <nvrhi/nvrhi.h>


struct GLFWwindow;

namespace Lynx
{
   
    
    class Renderer
    {
    public:
        Renderer(GLFWwindow* window);
        ~Renderer();
        
        void BeginFrame();
        void EndFrame();

    private:
        void InitVulkan(GLFWwindow* window);
        void InitNVRHI();
        void InitPipeline();

        // TODO: Pimpl idiom
        struct VulkanState;
        std::unique_ptr<VulkanState> m_VulkanState;
        
        nvrhi::DeviceHandle m_NvrhiDevice;
        nvrhi::CommandListHandle m_CommandList;
        nvrhi::ShaderHandle m_VertexShader;
        nvrhi::ShaderHandle m_FragmentShader;
        nvrhi::GraphicsPipelineHandle m_Pipeline;

        // One NVRHI framebuffer wrapper per swapchain image
        std::vector<nvrhi::FramebufferHandle> m_Framebuffers;
        uint32_t m_CurrentImageIndex = 0;
    };
}

