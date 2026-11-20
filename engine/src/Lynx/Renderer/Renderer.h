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


#include "RenderPass.h"
#include "Screenshot.h"
#include "Passes/BloomPass.h"
#include "Passes/MipMapBlitPass.h"

struct GLFWwindow;

namespace Lynx
{
    class FrameProfiler;
    class GraphicsContext;
    class RenderGraph;
    struct ExtractedScene;
    class Swapchain;
    struct ImGui_NVRHI;
    struct SwapchainSettings;
    
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
        bool IsVSync() const;

        // One window image more than the driver's minimum. With VSync it lets the CPU start the next frame
        // without waiting for the display, at the price of memory and of a frame that may wait in line.
        void SetExtraSwapchainImage(bool enabled);
        bool HasExtraSwapchainImage() const;

        // A frame is everything that ends up in the window at once: the scene, if one is drawn, and the ImGui layer.
        // BeginFrame returns false if the window cannot be drawn to right now. The frame is then skipped: do not call EndFrame.
        bool BeginFrame();
        void EndFrame();

        // Draws a scene into the frame. Call it between BeginFrame and EndFrame, at most once per frame.
        // Everything is read from the extracted copy, see SceneRenderer.
        void RenderScene(const ExtractedScene& scene);
        
        nvrhi::TextureHandle GetViewportTexture() const;
        std::pair<uint32_t, uint32_t> GetViewportSize() const;

        std::pair<nvrhi::BufferHandle, nvrhi::BufferHandle> CreateMeshBuffers(std::vector<Vertex> vertices, std::vector<uint32_t> indices);


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
        void BeginScene(const glm::mat4& view, const glm::mat4 projection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity, float deltaTime, bool editMode);
        void SubmitMesh(const std::shared_ptr<StaticMesh>& mesh, const glm::mat4& transform, RenderFlags flags, int entityID);
        void SubmitParticles(Material* material, const std::vector<ParticleInstanceData>& particles);
        void EndScene();

        void CreateDefaultTextures();
        void InitBuffers();
        void ApplySwapchainSettings(const SwapchainSettings& settings);
        void ReleaseSwapchainImages();
        void CreateRenderTarget(RenderTarget& target, uint32_t width, uint32_t height);
        void PrepareDrawCalls();
        void CopyFrameForScreenshots(nvrhi::ITexture* frame);
        void ReportMemoryUse();

    private:
        // Declared in this order because each one needs the ones before it, and they are destroyed in reverse.
        std::unique_ptr<GraphicsContext> m_Context;
        std::unique_ptr<Swapchain> m_Swapchain;

        struct FrameResources;
        std::unique_ptr<FrameResources> m_Frame;

        // The context's device. Kept as a member because every part of this class uses it.
        nvrhi::DeviceHandle m_NvrhiDevice;
        nvrhi::CommandListHandle m_CommandList;
        nvrhi::StagingTextureHandle m_StageBuffer;

        nvrhi::BufferHandle m_GlobalCB;

        nvrhi::TextureHandle m_WhiteTex;
        nvrhi::TextureHandle m_NormalTex;
        nvrhi::TextureHandle m_BlackTex;
        nvrhi::TextureHandle m_MetallicRoughnessTex;

        uint32_t m_CurrentFrame = 0;

        // The picture the scene of this frame ended up in. Null if no scene was drawn in this frame.
        nvrhi::TextureHandle m_SceneOutput;
        static const int MAX_FRAMES_IN_FLIGHT = 2;

        std::unique_ptr<RenderTarget> m_SceneTarget;
        std::unique_ptr<ImGui_NVRHI> m_ImGuiBackend;
        bool m_ShouldCreateIDTarget = false;
        
        std::unordered_map<SamplerSettings, nvrhi::SamplerHandle> m_SamplerCache;

        std::unordered_map<BatchKey, std::vector<GPUInstanceData>, BatchKeyHasher> m_OpaqueBatches;
        std::unordered_map<Material*, std::vector<ParticleInstanceData>> m_ParticleBatches;


        // Every pass of a frame, in the order they run.
        std::unique_ptr<RenderGraph> m_Graph;

        // Owned by the graph. Kept because its settings are edited from outside.
        BloomPass* m_BloomPass = nullptr;
        std::unique_ptr<MipMapBlitPass> m_MipMapGenPass;

        RenderContext m_RenderContext;
        RenderData m_CurrentFrameData;

        int m_ShadowMapResolution = 4096;

        bool m_ShowGrid = true;
        bool m_ShowColliders = false;
        bool m_ShowUI = true;
        bool m_FXAAEnabled = true;
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

