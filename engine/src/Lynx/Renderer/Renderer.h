#pragma once

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "Lynx/Asset/Texture.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/TextureSpecification.h"


#include "RenderPass.h"
#include "Screenshot.h"

struct GLFWwindow;

namespace Lynx
{
    class AssetManager;
    class GpuTexture;
    class MaterialTextures;
    class FrameProfiler;
    class GraphicsContext;
    class RenderGraph;
    struct ExtractedScene;
    class Swapchain;
    class ImGuiVulkan;
    struct SwapchainSettings;
    
    class LX_API Renderer
    {
    public:
        struct RenderStats
        {
            uint32_t DrawCalls = 0;
            uint32_t IndexCount = 0;
            float FrameTime = 0.0f;
        };
        
        Renderer(GLFWwindow* window, FrameProfiler& profiler, AssetManager& assets, bool initIDTarget = false);
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
        
        // The ImTextureID that ImGui::Image takes to show the finished scene picture, and to show a texture of
        // the texture table (see Texture::GetTableIndex). Zero if there is no scene picture yet.
        // Returned as a plain number so that this header does not need ImGui's.
        uint64_t GetViewportImGuiTexture() const;
        uint64_t GetImGuiTexture(uint32_t tableIndex) const;

        // Call before ImGui::NewFrame.
        void BeginImGuiFrame();

        // Has to run before the ImGui context is destroyed, which happens before the renderer is.
        void ShutdownImGui();
        std::pair<uint32_t, uint32_t> GetViewportSize() const;

        std::pair<MeshBuffer, MeshBuffer> CreateMeshBuffers(std::vector<Vertex> vertices, std::vector<uint32_t> indices);


        nvrhi::DeviceHandle GetDeviceHandle() const { return m_NvrhiDevice; }

        // Makes a texture on the GPU from pixels, with all its smaller sizes if the specification asks for them.
        // Null if the pixels are missing or the format cannot be used.
        std::unique_ptr<GpuTexture> CreateTexture(const TextureSpecification& specification, const unsigned char* data);

        // Asks which entity the scene picture shows at a pixel. The answer arrives a few frames later,
        // so that no frame has to wait for the GPU. A new request replaces one that is still under way.
        void RequestEntityIdAt(uint32_t x, uint32_t y);

        // The answer to the latest request, handed out once. Empty until it has arrived. -1 means no entity.
        std::optional<int> TakeEntityId();

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

        // Puts a texture into the table that shaders read from. The slot keeps the texture alive.
        TextureSlot AddToTextureTable(const GpuTexture& texture);

        // The index of the sampler with these settings in the sampler table.
        uint32_t SamplerIndex(const SamplerSettings& settings);

        BloomSettings& GetBloomSettings() { return m_BloomSettings; }
        const BloomSettings& GetBloomSettings() const { return m_BloomSettings; }

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
        void ApplySwapchainSettings(const SwapchainSettings& settings);
        void ReleaseSwapchainImages();
        void CreateSceneTargets(uint32_t width, uint32_t height);
        void PrepareDrawCalls();
        void CopyFrameForScreenshots(GpuTexture& frame);
        void CopyRequestedEntityId();
        void DrawImGui(GpuTexture& window);
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


        uint32_t m_CurrentFrame = 0;

        // The picture the scene of this frame ended up in. Null if no scene was drawn in this frame.
        GpuTexture* m_SceneOutput = nullptr;
        static const int MAX_FRAMES_IN_FLIGHT = 2;

        // The size of the pictures the scene is drawn into. Empty until they exist.
        std::optional<std::pair<uint32_t, uint32_t>> m_SceneSize;
        std::unique_ptr<ImGuiVulkan> m_ImGui;
        bool m_ShouldCreateIDTarget = false;
        

        std::unordered_map<BatchKey, std::vector<GPUInstanceData>, BatchKeyHasher> m_OpaqueBatches;
        std::unordered_map<Material*, std::vector<ParticleInstanceData>> m_ParticleBatches;


        // Every pass of a frame, in the order they run.
        std::unique_ptr<RenderGraph> m_Graph;

        BloomSettings m_BloomSettings;

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

        // For the textures of the materials that are drawn.
        AssetManager& m_Assets;
        std::unique_ptr<MaterialTextures> m_MaterialTextures;
        std::chrono::steady_clock::time_point m_SubmitStart;

        std::vector<uint64_t> m_ScreenshotRequests;
    };
}

