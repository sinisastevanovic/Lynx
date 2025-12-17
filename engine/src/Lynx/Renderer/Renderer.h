#pragma once

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>


struct GLFWwindow;

namespace Lynx
{
    class EditorCamera;
    
    class LX_API Renderer
    {
    public:
        struct SceneData
        {
            glm::mat4 ViewProjectionMatrix;
            float padding[48]; // Padding to 256 bytes (64 used, 192 remaining -> 48 floats)
        };
        
        Renderer(GLFWwindow* window);
        ~Renderer();
        
        void OnResize(uint32_t width, uint32_t height);

        void BeginScene(const EditorCamera& camera);
        void EndScene();

    private:
        void InitVulkan(GLFWwindow* window);
        void InitNVRHI();
        void InitPipeline();
        void InitBuffers();

        // TODO: Pimpl idiom
        struct VulkanState;
        std::unique_ptr<VulkanState> m_VulkanState;
        
        nvrhi::DeviceHandle m_NvrhiDevice;
        nvrhi::CommandListHandle m_CommandList;
        nvrhi::ShaderHandle m_VertexShader;
        nvrhi::ShaderHandle m_FragmentShader;
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::BufferHandle m_CubeVertexBuffer;
        nvrhi::BufferHandle m_CubeIndexBuffer;
        nvrhi::BufferHandle m_ConstantBuffer;
        nvrhi::BindingSetHandle m_BindingSet;
        nvrhi::TextureHandle m_DepthBuffer;
        

        // One NVRHI framebuffer wrapper per swapchain image
        std::vector<nvrhi::FramebufferHandle> m_Framebuffers;
        uint32_t m_CurrentImageIndex = 0;
        
        static const int MAX_FRAMES_IN_FLIGHT = 2;
        uint32_t m_CurrentFrame = 0;
    };
}

