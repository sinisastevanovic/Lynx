#pragma once

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "Lynx/Asset/Texture.h"
#include "Lynx/Asset/StaticMesh.h"


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

        void BeginScene(glm::mat4 viewProjection);
        void EndScene();
        
        void SubmitMesh(std::shared_ptr<StaticMesh> mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

        nvrhi::DeviceHandle GetDeviceHandle() const { return m_NvrhiDevice; }

        void SetTexture(std::shared_ptr<Texture> texture);

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
        nvrhi::BufferHandle m_ConstantBuffer;
        nvrhi::BindingSetHandle m_BindingSet;
        nvrhi::TextureHandle m_DepthBuffer;
        nvrhi::SamplerHandle m_Sampler;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::TextureHandle m_DefaultTexture;
        

        // One NVRHI framebuffer wrapper per swapchain image
        std::vector<nvrhi::FramebufferHandle> m_Framebuffers;
        uint32_t m_CurrentImageIndex = 0;
        
        static const int MAX_FRAMES_IN_FLIGHT = 2;
        uint32_t m_CurrentFrame = 0;
    };
}

