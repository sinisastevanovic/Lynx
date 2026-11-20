#pragma once

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include <chrono>
#include <string>
#include <vector>

#include "Passes/MipMapGenPass.h"
#include "Lynx/Asset/Texture.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/TextureSpecification.h"

#include "RenderPipeline.h"
#include "RenderPass.h"
#include "Screenshot.h"
#include "Lynx/UI/Rendering/UIPass.h"
#include "Passes/BloomPass.h"
#include "Passes/CompositePass.h"
#include "Passes/MipMapBlitPass.h"

struct GLFWwindow;

namespace Lynx
{
    class FrameProfiler;
    struct ImGui_NVRHI;
    
    class LX_API Renderer
    {
    public:
        struct RenderTarget
        {
            // HDR Scene Color (RGBA16_FLOAT)
            nvrhi::TextureHandle Color;
            // Depth Buffer (D32)
            nvrhi::TextureHandle Depth;
            // LDR Final Color (BGRA8_UNORM)
            nvrhi::TextureHandle Output;
            // Framebuffer for ForwardPass (Color + Depth)
            nvrhi::FramebufferHandle HDRFramebuffer;
            // Framebuffer for CompositePass (Output)
            nvrhi::FramebufferHandle LDRFramebuffer;
            // ID buffer for picking
            nvrhi::TextureHandle IdBuffer;
            uint32_t Width = 0;
            uint32_t Height = 0;
        };

        struct RenderStats
        {
            uint32_t DrawCalls = 0;
            uint32_t IndexCount = 0;
            float FrameTime = 0.0f;
        };
        
        Renderer(GLFWwindow* window, FrameProfiler& profiler, bool initIDTarget = false);
        ~Renderer();

        void Init();
        
        void OnResize(uint32_t width, uint32_t height);
        void EnsureEditorViewport(uint32_t width, uint32_t height);

        // With VSync on, frames are presented in step with the display refresh.
        // Off presents as fast as the GPU allows, without tearing where the driver supports it.
        void SetVSync(bool enabled);
        bool IsVSync() const { return m_VSync; }

        void BeginScene(const glm::mat4& view, const glm::mat4 projection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity, float deltaTime, bool editMode = false);
        void EndScene();
        
        nvrhi::TextureHandle GetViewportTexture() const;
        std::pair<uint32_t, uint32_t> GetViewportSize() const;

        std::pair<nvrhi::BufferHandle, nvrhi::BufferHandle> CreateMeshBuffers(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
        void SubmitMesh(std::shared_ptr<StaticMesh> mesh, const glm::mat4& transform, RenderFlags flags, int entityID = -1);
        void SubmitParticles(Material* material, const std::vector<ParticleInstanceData>& particles);

        nvrhi::DeviceHandle GetDeviceHandle() const { return m_NvrhiDevice; }

        nvrhi::TextureHandle CreateTexture(const TextureSpecification& specification, unsigned char* data);

        int ReadIdFromBuffer(uint32_t x, uint32_t y);

        void SetShowGrid(bool show) { m_ShowGrid = show; }
        bool GetShowGrid() const { return m_ShowGrid; }

        void SetShowColliders(bool show) { m_ShowColliders = show; }
        bool GetShowColliders() const { return m_ShowColliders; }

        void SetShowUI(bool show) { m_ShowUI = show; }
        bool GetShowUI() const { return m_ShowUI; }

        const RenderStats& GetRenderStats() const { return m_Stats; }
        void ResetStats();

        // Copies the picture of the frame that is rendered next, without the ImGui layer.
        // The copy is read back a few frames later, so asking for it does not stall the frame.
        void RequestScreenshot(uint64_t tag);

        // Returns the screenshots that have arrived. With waitForAll, waits for the GPU and returns every one requested.
        std::vector<Screenshot> TakeScreenshots(bool waitForAll = false);

        // Waits for the GPU and reports the GPU times of the frames that are still outstanding.
        void FlushGpuTimers();

        std::string GetAdapterName() const;

        glm::mat4 GetLightViewProjMatrix() const { return m_CurrentFrameData.LightViewProj; }
        glm::mat4 GetCameraViewProjMatrix() const { return m_CurrentFrameData.ViewProjection; }

        BloomSettings& GetBloomSettings() { return m_BloomPass->GetSettings(); }
        const BloomSettings& GetBloomSettings() const { return m_BloomPass->GetSettings(); }

        void SetFXAAEnabled(bool enabled) { m_FXAAEnabled = enabled; }
        bool GetFXAAEnabled() const { return m_FXAAEnabled; }

        void SetMaxAnisotropy(float maxAnisotropy) { m_MaxAnisotropy = maxAnisotropy; }
        float GetMaxAnisotropy() const { return m_MaxAnisotropy; }

    private:
        void InitVulkan(GLFWwindow* window);
        void InitNVRHI();
        void InitBuffers();
        void CreateSwapchain(uint32_t fallbackWidth, uint32_t fallbackHeight);
        void CreateSwapchainFramebuffers();
        void RecreateSwapchain(uint32_t width, uint32_t height);
        void CreateRenderTarget(RenderTarget& target, uint32_t width, uint32_t height);
        void PrepareDrawCalls();
        void CopyFrameForScreenshots(nvrhi::ITexture* frame);
        void ReportMemoryUse();

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
        uint32_t m_CurrentImageIndex = 0;
        uint32_t m_CurrentFrame = 0;
        static const int MAX_FRAMES_IN_FLIGHT = 2;

        std::unique_ptr<RenderTarget> m_SceneTarget;
        std::unique_ptr<ImGui_NVRHI> m_ImGuiBackend;
        bool m_ShouldCreateIDTarget = false;
        
        std::unordered_map<SamplerSettings, nvrhi::SamplerHandle> m_SamplerCache;

        std::unordered_map<BatchKey, std::vector<GPUInstanceData>, BatchKeyHasher> m_OpaqueBatches;
        std::unordered_map<Material*, std::vector<ParticleInstanceData>> m_ParticleBatches;
        nvrhi::BufferHandle m_ParticleInstanceBuffer;

        RenderPipeline m_Pipeline;
        std::unique_ptr<CompositePass> m_CompositePass;
        std::unique_ptr<BloomPass> m_BloomPass;
        std::unique_ptr<MipMapBlitPass> m_MipMapGenPass;
        std::unique_ptr<UIPass> m_UIPass;
        RenderContext m_RenderContext;
        RenderData m_CurrentFrameData;

        int m_ShadowMapResolution = 4096;

        bool m_ShowGrid = true;
        bool m_ShowColliders = false;
        bool m_ShowUI = true;
        bool m_FXAAEnabled = true;
        bool m_VSync = true;
        float m_MaxAnisotropy = 16.0f;

        RenderStats m_Stats;

        FrameProfiler& m_Profiler;
        std::chrono::steady_clock::time_point m_SubmitStart;

        struct PendingScreenshot
        {
            uint64_t Tag = 0;
            uint64_t Frame = 0;
            nvrhi::StagingTextureHandle Staging;
        };
        std::vector<uint64_t> m_ScreenshotRequests;
        std::vector<PendingScreenshot> m_PendingScreenshots;
    };
}

