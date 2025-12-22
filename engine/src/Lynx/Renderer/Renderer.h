#pragma once

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "Lynx/Asset/Texture.h"
#include "Lynx/Asset/StaticMesh.h"

struct GLFWwindow;

namespace Lynx
{
    struct ImGui_NVRHI;
    
    class LX_API Renderer
    {
    public:
        struct SceneData
        {
            glm::mat4 ViewProjectionMatrix;
            float padding[48]; // Padding to 256 bytes (64 used, 192 remaining -> 48 floats)
        };

        struct RenderTarget
        {
            nvrhi::TextureHandle Color;
            nvrhi::TextureHandle Depth;
            nvrhi::TextureHandle IdBuffer;
            nvrhi::FramebufferHandle Framebuffer;
            uint32_t Width = 0;
            uint32_t Height = 0;
        };
        
        Renderer(GLFWwindow* window);
        ~Renderer();
        
        void OnResize(uint32_t width, uint32_t height);
        
        void EnsureEditorViewport(uint32_t width, uint32_t height);
        nvrhi::TextureHandle GetViewportTexture() const;
        std::pair<uint32_t, uint32_t> GetViewportSize() const;

        void BeginScene(glm::mat4 viewProjection);
        void EndScene();
        
        void SubmitMesh(std::shared_ptr<StaticMesh> mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f), int entityID = -1);

        nvrhi::DeviceHandle GetDeviceHandle() const { return m_NvrhiDevice; }

        void SetTexture(std::shared_ptr<Texture> texture);

        bool InitImGui();
        void RenderImGui();

        int ReadIdFromBuffer(uint32_t x, uint32_t y);

    private:
        void InitVulkan(GLFWwindow* window);
        void InitNVRHI();
        void InitPipeline();
        void InitBuffers();

        void CreateRenderTarget(RenderTarget& target, uint32_t width, uint32_t height);

    private:
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
        nvrhi::SamplerHandle m_Sampler;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::TextureHandle m_DefaultTexture;
        nvrhi::StagingTextureHandle m_StageBuffer;

        // One NVRHI framebuffer wrapper per swapchain image
        std::vector<nvrhi::FramebufferHandle> m_SwapchainFramebuffers;
        nvrhi::TextureHandle m_SwapchainDepth;
        uint32_t m_CurrentImageIndex = 0;

        std::unique_ptr<RenderTarget> m_EditorTarget;
        nvrhi::FramebufferHandle m_CurrentFramebuffer;
        
        std::unique_ptr<ImGui_NVRHI> m_ImGuiBackend;

        static const int MAX_FRAMES_IN_FLIGHT = 2;
        uint32_t m_CurrentFrame = 0;
    };
}

