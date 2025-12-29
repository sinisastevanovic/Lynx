#pragma once

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "Lynx/Asset/Texture.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/TextureSpecification.h"

#include "RenderPipeline.h"
#include "RenderPass.h"

struct GLFWwindow;

namespace Lynx
{
    struct ImGui_NVRHI;
    
    class LX_API Renderer
    {
    public:
        struct RenderTarget
        {
            nvrhi::TextureHandle Color;
            nvrhi::TextureHandle Depth;
            nvrhi::TextureHandle IdBuffer;
            nvrhi::FramebufferHandle Framebuffer;
            uint32_t Width = 0;
            uint32_t Height = 0;
        };

        struct RenderStats
        {
            uint32_t DrawCalls = 0;
            uint32_t IndexCount = 0;
            float FrameTime = 0.0f;
        };
        
        Renderer(GLFWwindow* window, bool initIDTarget = false);
        ~Renderer();

        void Init();
        void ReloadShaders();
        
        void OnResize(uint32_t width, uint32_t height);
        void EnsureEditorViewport(uint32_t width, uint32_t height);

        void BeginScene(const glm::mat4& view, const glm::mat4 projection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity, float deltaTime, bool editMode = false);
        void EndScene();
        
        nvrhi::TextureHandle GetViewportTexture() const;
        std::pair<uint32_t, uint32_t> GetViewportSize() const;

        std::pair<nvrhi::BufferHandle, nvrhi::BufferHandle> CreateMeshBuffers(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
        void SubmitMesh(std::shared_ptr<StaticMesh> mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f), int entityID = -1);

        nvrhi::DeviceHandle GetDeviceHandle() const { return m_NvrhiDevice; }

        nvrhi::SamplerHandle GetSampler(const SamplerSettings& settings);
        nvrhi::TextureHandle CreateTexture(const TextureSpecification& specification, unsigned char* data);

        int ReadIdFromBuffer(uint32_t x, uint32_t y);

        void SetShowGrid(bool show) { m_ShowGrid = show; }
        bool GetShowGrid() const { return m_ShowGrid; }

        void SetShowColliders(bool show) { m_ShowColliders = show; }
        bool GetShowColliders() const { return m_ShowColliders; }

        const RenderStats& GetRenderStats() const { return m_Stats; }
        void ResetStats();

    private:
        void InitVulkan(GLFWwindow* window);
        void InitNVRHI();
        void InitBuffers();
        void CreateRenderTarget(RenderTarget& target, uint32_t width, uint32_t height);
        void PrepareDrawCalls();

    private:
        // TODO: Pimpl idiom
        struct VulkanState;
        std::unique_ptr<VulkanState> m_VulkanState;
        
        nvrhi::DeviceHandle m_NvrhiDevice;
        nvrhi::CommandListHandle m_CommandList;
        nvrhi::StagingTextureHandle m_StageBuffer;

        nvrhi::BufferHandle m_GlobalCB;
        nvrhi::BufferHandle m_InstanceBuffer;
        nvrhi::TextureHandle m_WhiteTex;
        nvrhi::TextureHandle m_NormalTex;
        nvrhi::TextureHandle m_BlackTex;
        nvrhi::TextureHandle m_MetallicRoughnessTex;

        // One NVRHI framebuffer wrapper per swapchain image
        std::vector<nvrhi::FramebufferHandle> m_SwapchainFramebuffers;
        nvrhi::TextureHandle m_SwapchainDepth;
        uint32_t m_CurrentImageIndex = 0;
        uint32_t m_CurrentFrame = 0;
        static const int MAX_FRAMES_IN_FLIGHT = 2;

        std::unique_ptr<RenderTarget> m_EditorTarget;
        std::unique_ptr<ImGui_NVRHI> m_ImGuiBackend;
        bool m_ShouldCreateIDTarget = false;
        
        std::unordered_map<SamplerSettings, nvrhi::SamplerHandle> m_SamplerCache;

        RenderPipeline m_Pipeline;
        RenderContext m_RenderContext;
        RenderData m_CurrentFrameData;

        int m_ShadowMapResolution = 4096;

        bool m_ShowGrid = true;
        bool m_ShowColliders = false;

        RenderStats m_Stats;
    };
}

