#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <span>
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
    class GpuBuffer;
    class GpuScene;
    class TerrainRenderer;
    struct ExtractedTerrain;
    class GpuTexture;
    class MaterialTable;
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
        // The meshes are those in GetGpuScene. Everything else is read from the extracted copy, see SceneRenderer.
        void RenderScene(const ExtractedScene& scene);

        // The meshes that RenderScene draws. Whoever feeds the renderer keeps it in step with the scene.
        GpuScene& GetGpuScene() { return *m_GpuScene; }

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

        // Puts the vertices and indices of a submesh into the geometry pool. They can be drawn from the next frame on.
        MeshGeometry CreateMeshGeometry(std::span<const PackedVertex> vertices, std::span<const uint32_t> indices);


        // Puts what the vertices of a foliage submesh know about their leaves on the GPU, one entry per
        // vertex. Shaders reach it through its address, which an instance of the submesh carries.
        std::shared_ptr<const GpuBuffer> CreateLeafData(std::span<const LeafVertex> leaves);

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

        glm::mat4 GetCameraViewProjMatrix() const { return m_CurrentFrameData.ViewProjection; }

        // Puts a texture into the table that shaders read from. The slot keeps the texture alive.
        TextureSlot AddToTextureTable(const GpuTexture& texture);

        // Puts the texture in a second time, read as sRGB: what a shader samples there is linear light.
        // An empty slot for a texture whose format has no sRGB form.
        TextureSlot AddColorToTextureTable(const GpuTexture& texture);

        // The index of the sampler with these settings in the sampler table.
        uint32_t SamplerIndex(const SamplerSettings& settings);

        BloomSettings& GetBloomSettings() { return m_BloomSettings; }
        const BloomSettings& GetBloomSettings() const { return m_BloomSettings; }

        ShadowSettings& GetShadowSettings() { return m_ShadowSettings; }
        const ShadowSettings& GetShadowSettings() const { return m_ShadowSettings; }

        TemporalSettings& GetTemporalSettings() { return m_TemporalSettings; }
        const TemporalSettings& GetTemporalSettings() const { return m_TemporalSettings; }

        LodSettings& GetLodSettings() { return m_LodSettings; }
        const LodSettings& GetLodSettings() const { return m_LodSettings; }

        void SetFXAAEnabled(bool enabled) { m_FXAAEnabled = enabled; }
        bool GetFXAAEnabled() const { return m_FXAAEnabled; }

        void SetMaxAnisotropy(float maxAnisotropy) { m_MaxAnisotropy = maxAnisotropy; }
        float GetMaxAnisotropy() const { return m_MaxAnisotropy; }

    private:
        void BeginScene(const glm::mat4& view, const glm::mat4 projection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity, float deltaTime, bool editMode);
        void SubmitParticles(Material* material, const std::vector<ParticleInstanceData>& particles);
        void EndScene();

        void CreateDefaultTextures();
        void CreateShadowMaps();
        void CreateAtmosphereTables();
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


        uint32_t m_CurrentFrame = 0;

        // The picture the scene of this frame ended up in. Null if no scene was drawn in this frame.
        GpuTexture* m_SceneOutput = nullptr;
        static const int MAX_FRAMES_IN_FLIGHT = 2;

        // The size of the pictures the scene is drawn into. Empty until they exist.
        std::optional<std::pair<uint32_t, uint32_t>> m_SceneSize;
        std::unique_ptr<ImGuiVulkan> m_ImGui;
        bool m_ShouldCreateIDTarget = false;


        std::unordered_map<Material*, std::vector<ParticleInstanceData>> m_ParticleBatches;


        // Every pass of a frame, in the order they run.
        std::unique_ptr<RenderGraph> m_Graph;

        BloomSettings m_BloomSettings;
        LodSettings m_LodSettings;
        ShadowSettings m_ShadowSettings;
        ShadowFit m_ShadowFit;

        // The camera's matrix as shaders got it in the last frame, without jitter. Empty before the first frame.
        std::optional<glm::mat4> m_PreviousClipViewProjection;

        TemporalSettings m_TemporalSettings;

        // How many frames have been jittered, which says how the next one is.
        uint64_t m_JitterFrame = 0;
        std::unique_ptr<TerrainRenderer> m_Terrain;

        // The terrains of the scene that is being drawn. Valid only while it is.
        std::span<const ExtractedTerrain> m_SceneTerrains;

        RenderContext m_RenderContext;
        RenderData m_CurrentFrameData;


        bool m_ShowGrid = true;
        bool m_ShowColliders = false;
        bool m_ShowUI = true;
        bool m_FXAAEnabled = true;
        float m_MaxAnisotropy = 16.0f;

        RenderStats m_Stats;

        FrameProfiler& m_Profiler;

        // For the textures of the materials that are drawn.
        AssetManager& m_Assets;
        std::unique_ptr<MaterialTable> m_Materials;

        // The meshes of the scene that is drawn, kept on the GPU across frames.
        std::unique_ptr<GpuScene> m_GpuScene;

        std::vector<uint64_t> m_ScreenshotRequests;
    };
}

