#include "Renderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>

#include "ExtractedScene.h"
#include "Frustum.h"
#include "Graph/RenderGraph.h"
#include "Passes/CompositePass.h"
#include "Lynx/UI/Rendering/UIPass.h"
#include "GpuScene.h"
#include "GpuTimers.h"
#include "GraphicsContext.h"
#include "SamplerCache.h"
#include "Swapchain.h"
#include "Vulkan/GeometryPool.h"
#include "Vulkan/GpuBuffer.h"
#include "Vulkan/GpuTexture.h"
#include "Vulkan/Rendering.h"
#include "Vulkan/SharedBindings.h"
#include "Vulkan/UploadQueue.h"
#include "Lynx/Profiling/FrameProfiler.h"
#include "Passes/DebugPass.h"
#include "Passes/ForwardPass.h"
#include "Lynx/Asset/AssetManager.h"

#include <bit>
#include "MaterialTable.h"
#include "Passes/BloomPass.h"
#include "Passes/GridPass.h"
#include "RenderFormats.h"
#include "Passes/ShadowPass.h"
#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "Passes/CullPass.h"
#include "Passes/DepthPass.h"
#include "Passes/DepthPyramidPass.h"
#include "Passes/ParticlePass.h"


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


#include "Lynx/ImGui/ImGuiVulkan.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>


namespace Lynx
{
    namespace Helpers
    {
        vk::Format ToVulkan(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::RGBA8: return vk::Format::eR8G8B8A8Unorm;
                case TextureFormat::RG16F: return vk::Format::eR16G16Sfloat;
                case TextureFormat::RG32F: return vk::Format::eR32G32Sfloat;
                case TextureFormat::R32I:  return vk::Format::eR32Uint;
                case TextureFormat::R8:    return vk::Format::eR8Unorm;

                // Depth formats are for render targets, which are not made from pixels.
                case TextureFormat::Depth32:
                case TextureFormat::Depth24Stencil8:
                    break;
            }
            return vk::Format::eUndefined;
        }

        static uint32_t GetTextureFormatByteSize(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::RGBA8:          return 4;
                case TextureFormat::RG16F:          return 4; // 2 * 2 bytes (half float)
                case TextureFormat::RG32F:          return 8; // 2 * 4 bytes (float)
                case TextureFormat::R32I:           return 4; // 1 * 4 bytes (int)
                case TextureFormat::Depth32:        return 4;
                case TextureFormat::Depth24Stencil8:return 4;
                case TextureFormat::R8:             return 1;
            }
            LX_CORE_WARN("Unknown Texture Format byte size, defaulting to 4");
            return 4;
        }
    }

    namespace
    {
        // A screenshot is read this many frames after it was copied, when the GPU has certainly finished the copy.
        constexpr uint64_t ReadbackDelay = 3;
    }

    // What a frame needs from Vulkan directly. Two frames can be under way at once:
    // the CPU records one while the GPU still draws the one before.
    struct Renderer::FrameResources
    {
        std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> ImageAvailable;
        std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> InFlight;

        // A command buffer per frame that can be under way, each from a pool of its own, so that starting
        // a frame gives all of the last one's memory back in one call.
        std::array<vk::CommandPool, MAX_FRAMES_IN_FLIGHT> CommandPools;
        std::array<vk::CommandBuffer, MAX_FRAMES_IN_FLIGHT> CommandBuffers;

        // The command buffer of the frame that is being recorded.
        vk::CommandBuffer Commands;
        std::unique_ptr<GpuTimers> Timers;
        std::unique_ptr<UploadQueue> Uploads;
        std::unique_ptr<GeometryPool> Geometry;
        std::unique_ptr<SharedBindings> Shared;

        // Where the texture table has the scene's HDR picture and the black default texture.
        TextureSlot SceneColorSlot;
        TextureSlot SceneDepthSlot;

        // The finished scene picture, which the editor shows in a panel through ImGui.
        TextureSlot SceneOutputSlot;

        // Picking: the pixel that was asked for, the buffer its entity ID is copied into, the frame the copy
        // was recorded in, and an answer that needed no copy.
        std::optional<glm::uvec2> PickRequest;
        GpuBuffer PickReadback;
        std::optional<uint64_t> PickCopiedInFrame;
        std::optional<int> PickedEntityId;
        uint32_t BlackTextureIndex = 0;
        uint32_t WhiteTextureIndex = 0;
        uint32_t NormalTextureIndex = 0;
        uint32_t MetallicRoughnessTextureIndex = 0;
        GpuTexture BlackTexture;
        GpuTexture WhiteTexture;
        GpuTexture NormalTexture;
        GpuTexture MetallicRoughnessTexture;

        // A buffer that is rewritten every frame with a list whose length changes, which shaders read
        // through the buffer's address.
        struct FrameList
        {
            GpuBuffer Buffer;

            // Makes sure the buffer can hold the entries, then queues them for upload.
            template<typename Entry>
            void Upload(GraphicsContext& context, UploadQueue& uploads, std::span<const Entry> instances, vk::BufferUsageFlagBits usage, const char* name)
            {
                const uint64_t requiredSize = instances.size_bytes();
                if (requiredSize == 0)
                    return;

                if (Buffer.Size() < requiredSize)
                {
                    // Half as much again as needed, so a scene that grows a little each frame does not reallocate each frame.
                    GpuBufferDesc desc;
                    desc.Size = requiredSize + requiredSize / 2;
                    desc.Usage = usage | vk::BufferUsageFlagBits::eTransferDst;
                    desc.Name = name;
                    Buffer = GpuBuffer(context, desc);
                }

                uploads.Upload(Buffer.Handle(), 0, std::as_bytes(instances));
            }
        };
        // The camera and the sun of the scene being drawn, which every shader can read. Rewritten for every scene.
        GpuBuffer SceneConstants;

        FrameList ParticleInstances;

        // A copy of a finished frame that is waiting to be read back.
        struct PendingScreenshot
        {
            uint64_t Tag = 0;
            uint64_t Frame = 0;
            uint32_t Width = 0;
            uint32_t Height = 0;
            GpuBuffer Pixels;
        };
        std::vector<PendingScreenshot> PendingScreenshots;

        // The pictures a scene is drawn into.
        GpuTexture SceneColor;
        GpuTexture SceneDepth;
        GpuTexture SceneOutput;
        GpuTexture SceneEntityIds;
    };

    Renderer::Renderer(GLFWwindow* window, FrameProfiler& profiler, AssetManager& assets, bool initIdTarget)
        : m_ShouldCreateIDTarget(initIdTarget), m_Profiler(profiler), m_Assets(assets)
    {
        m_Context = std::make_unique<GraphicsContext>(window);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        m_Swapchain = std::make_unique<Swapchain>(*m_Context, static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight), SwapchainSettings());

        m_Frame = std::make_unique<FrameResources>();
        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            // The fences start signalled, because the first frames wait for them before anything was submitted.
            m_Frame->ImageAvailable[frame] = m_Context->Device().createSemaphore(vk::SemaphoreCreateInfo());
            m_Frame->InFlight[frame] = m_Context->Device().createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

            m_Frame->CommandPools[frame] = m_Context->Device().createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient, m_Context->GraphicsQueueFamily()));
            m_Frame->CommandBuffers[frame] = m_Context->Device().allocateCommandBuffers(vk::CommandBufferAllocateInfo(m_Frame->CommandPools[frame], vk::CommandBufferLevel::ePrimary, 1)).front();
        }
        m_Frame->Timers = std::make_unique<GpuTimers>(m_Context->PhysicalDevice(), m_Context->Device(), m_Context->GraphicsQueueFamily());
        m_Frame->Uploads = std::make_unique<UploadQueue>(*m_Context);
        m_Frame->Geometry = std::make_unique<GeometryPool>(*m_Context, *m_Frame->Uploads, static_cast<uint32_t>(sizeof(PackedVertex)));
        m_Graph = std::make_unique<RenderGraph>(m_Profiler, *m_Frame->Timers);

        GpuBufferDesc sceneConstantsDesc;
        sceneConstantsDesc.Size = sizeof(SceneData);
        sceneConstantsDesc.Usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
        sceneConstantsDesc.Name = "SceneConstants";
        m_Frame->SceneConstants = GpuBuffer(*m_Context, sceneConstantsDesc);
        m_Frame->Shared = std::make_unique<SharedBindings>(*m_Context, m_Frame->SceneConstants.Handle());
        CreateDefaultTextures();
    }

    Renderer::~Renderer()
    {
        LX_CORE_INFO("Renderer shutting down...");

        m_Context->WaitIdle();

        // Everything made from the device goes before the device does.
        ReleaseSwapchainImages();
        m_Frame->ParticleInstances = {};
        m_Frame->PickReadback = {};
        m_Frame->SceneConstants = {};
        m_Frame->Uploads.reset();
        m_Frame->PendingScreenshots.clear();
        m_Graph.reset();
        m_GpuScene.reset();
        m_Materials.reset();
        m_Frame->SceneColorSlot = TextureSlot();
        m_Frame->SceneDepthSlot = TextureSlot();
        m_Frame->SceneOutputSlot = TextureSlot();
        m_Frame->Shared.reset();
        m_RenderContext = RenderContext();
        m_CurrentFrameData = RenderData();

        // After the scene's instances, which hold meshes whose geometry lives in the pool.
        m_Frame->Geometry.reset();

        SamplerCache::Shutdown();
        m_Frame->BlackTexture = {};
        m_Frame->WhiteTexture = {};
        m_Frame->NormalTexture = {};
        m_Frame->MetallicRoughnessTexture = {};
        m_Frame->SceneColor = {};
        m_Frame->SceneDepth = {};
        m_Frame->SceneOutput = {};
        m_Frame->SceneEntityIds = {};
        m_ImGui.reset();

        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            m_Context->Device().destroySemaphore(m_Frame->ImageAvailable[frame]);
            m_Context->Device().destroyFence(m_Frame->InFlight[frame]);
            m_Context->Device().destroyCommandPool(m_Frame->CommandPools[frame]);
        }
        m_Frame.reset();

        m_Swapchain.reset();
        m_Context.reset();
    }

    void Renderer::Init()
    {
        // TODO: Move these members to the struct, useless to have duplicates!
        m_RenderContext.Graphics = m_Context.get();
        m_RenderContext.Uploads = m_Frame->Uploads.get();
        m_RenderContext.Shared = m_Frame->Shared.get();
        m_RenderContext.WritesEntityIds = m_ShouldCreateIDTarget;

        SamplerCache::Init(m_Context->Device(), m_MaxAnisotropy);

        MaterialTextureIndices defaults;
        defaults.Albedo = m_Frame->WhiteTextureIndex;
        defaults.Normal = m_Frame->NormalTextureIndex;
        defaults.MetallicRoughness = m_Frame->MetallicRoughnessTextureIndex;
        defaults.Emissive = m_Frame->WhiteTextureIndex;
        defaults.Sampler = SamplerIndex(SamplerSettings());
        m_Materials = std::make_unique<MaterialTable>(*m_Context, *m_Frame->Uploads, m_Assets, defaults);
        m_GpuScene = std::make_unique<GpuScene>(*m_Context, *m_Frame->Uploads, *m_Materials);

        // The frame, start to finish. Culling decides what the scene passes draw, so it comes first.
        // It has two phases: what the camera saw in the last frame is drawn into the depth buffer, and
        // everything else is then tested against that depth, so that what is hidden is never drawn.
        // The scene passes draw the HDR picture, bloom reads it and composite
        // turns both into the output. What is not part of the scene is drawn on top of the output, so that
        // post-processing does not touch it: the grid and debug lines, still hidden by the scene's depth, then the UI.
        m_Graph->Add("CullMeshes", std::make_unique<CullPass>(CullPhase::Early));
        m_Graph->Add("Shadow", std::make_unique<ShadowPass>(m_ShadowMapResolution));
        m_Graph->Add("Depth", std::make_unique<DepthPass>(CullPhase::Early));
        m_Graph->Add("DepthPyramid", std::make_unique<DepthPyramidPass>());
        m_Graph->Add("CullOccluded", std::make_unique<CullPass>(CullPhase::Late));
        m_Graph->Add("DepthLate", std::make_unique<DepthPass>(CullPhase::Late));
        m_Graph->Add("Forward", std::make_unique<ForwardPass>());
        m_Graph->Add("Particles", std::make_unique<ParticlePass>());
        m_Graph->Add("Bloom", std::make_unique<BloomPass>());
        m_Graph->Add("Composite", std::make_unique<CompositePass>());
        if (m_ShouldCreateIDTarget)
            m_Graph->Add("Grid", std::make_unique<GridPass>());
        m_Graph->Add("Debug", std::make_unique<DebugPass>());
        m_Graph->Add("UI", std::make_unique<UIPass>(), [this]() { return m_ShowUI; });
        m_Graph->Init(m_RenderContext);

        m_ImGui = std::make_unique<ImGuiVulkan>(*m_Context, *m_Frame->Shared, RenderFormats::Output, SamplerCache::Get()->GetSampler(SamplerSettings()));

        LX_CORE_INFO("Renderer initialized successfully (Pipeline loaded).");
    }

    std::unique_ptr<GpuTexture> Renderer::CreateTexture(const TextureSpecification& specification, const unsigned char* data)
    {
        if (!data)
            return nullptr;

        const vk::Format format = Helpers::ToVulkan(specification.Format);
        if (format == vk::Format::eUndefined)
        {
            LX_CORE_ERROR("Texture '{0}' has a format that cannot be filled from pixels.", specification.DebugName);
            return nullptr;
        }

        GpuTextureDesc desc;
        desc.Width = specification.Width;
        desc.Height = specification.Height;
        desc.Format = format;

        // Whether a texture holds colours painted for the screen or plain numbers (a normal map, roughness)
        // depends on what it is used for, and one texture can be both: a material's colour and a UI picture.
        // So every texture offers both readings, and the user picks one.
        desc.SrgbView = true;

        // A greyscale file, such as a roughness map saved on its own, is stored with one channel.
        desc.ReadAsGrey = specification.Format == TextureFormat::R8;

        // Smaller sizes are made by copying from the size above, so the texture is both read from and written to.
        desc.Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
        desc.Name = specification.DebugName.c_str();

        const vk::FormatFeatureFlags supported = m_Context->PhysicalDevice().getFormatProperties(format).optimalTilingFeatures;

        // Nothing is ever drawn into the texture, but it is marked as if it could be. Measured on an RTX 5080
        // (driver of November 2026): the forward pass samples textures 6 % faster in the sponza benchmark with
        // the mark than without, at the price of about 3 % more video memory. The reason is inside the driver.
        // Measure again when textures become block-compressed, which cannot have the mark.
        if (supported & vk::FormatFeatureFlagBits::eColorAttachment)
            desc.Usage |= vk::ImageUsageFlagBits::eColorAttachment;

        if (specification.GenerateMips)
        {
            // Scaling down averages pixels, which not every format allows: whole numbers cannot be averaged.
            const vk::FormatFeatureFlags needed = vk::FormatFeatureFlagBits::eSampledImageFilterLinear | vk::FormatFeatureFlagBits::eBlitSrc | vk::FormatFeatureFlagBits::eBlitDst;
            if ((supported & needed) == needed)
                desc.MipLevels = static_cast<uint32_t>(std::bit_width(std::max(specification.Width, specification.Height)));
        }

        auto texture = std::make_unique<GpuTexture>(*m_Context, desc);

        const size_t byteCount = static_cast<size_t>(specification.Width) * specification.Height * Helpers::GetTextureFormatByteSize(specification.Format);
        m_Frame->Uploads->UploadTexture(*texture, std::as_bytes(std::span<const unsigned char>(data, byteCount)));
        return texture;
    }

    void Renderer::RequestEntityIdAt(uint32_t x, uint32_t y)
    {
        m_Frame->PickRequest = glm::uvec2(x, y);
        m_Frame->PickCopiedInFrame.reset();
    }

    void Renderer::CopyRequestedEntityId()
    {
        if (!m_Frame->PickRequest || !m_Frame->SceneEntityIds)
            return;

        const glm::uvec2 pixel = *m_Frame->PickRequest;
        m_Frame->PickRequest.reset();

        // The scene picture may have been resized since the request was made.
        if (pixel.x >= m_Frame->SceneEntityIds.Desc().Width || pixel.y >= m_Frame->SceneEntityIds.Desc().Height)
        {
            m_Frame->PickedEntityId = -1;
            return;
        }

        if (!m_Frame->PickReadback)
        {
            GpuBufferDesc desc;
            desc.Size = sizeof(int32_t);
            desc.Usage = vk::BufferUsageFlagBits::eTransferDst;
            desc.CpuReadable = true;
            desc.Name = "PickedEntityId";
            m_Frame->PickReadback = GpuBuffer(*m_Context, desc);
        }

        const vk::CommandBuffer commands = m_Frame->Commands;
        m_Frame->SceneEntityIds.Transition(commands, vk::ImageLayout::eTransferSrcOptimal);

        vk::BufferImageCopy region;
        region.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        region.imageOffset = vk::Offset3D(static_cast<int32_t>(pixel.x), static_cast<int32_t>(pixel.y), 0);
        region.imageExtent = vk::Extent3D(1, 1, 1);
        commands.copyImageToBuffer(m_Frame->SceneEntityIds.Image(), vk::ImageLayout::eTransferSrcOptimal, m_Frame->PickReadback.Handle(), region);

        m_Frame->PickCopiedInFrame = m_Profiler.CurrentFrame();
    }

    std::optional<int> Renderer::TakeEntityId()
    {
        if (m_Frame->PickedEntityId)
            return std::exchange(m_Frame->PickedEntityId, std::nullopt);

        // The copy is part of a frame the GPU finishes a little later. After this many frames it is done for certain.
        if (!m_Frame->PickCopiedInFrame || m_Profiler.CurrentFrame() < *m_Frame->PickCopiedInFrame + ReadbackDelay)
            return std::nullopt;

        m_Frame->PickCopiedInFrame.reset();
        return *static_cast<const int32_t*>(m_Frame->PickReadback.Data());
    }

    void Renderer::ResetStats()
    {
        m_CurrentFrameData.DrawCalls = 0;
        m_CurrentFrameData.IndexCount = 0;
        memset(&m_Stats, 0, sizeof(RenderStats));
    }

    void Renderer::SetVSync(bool enabled)
    {
        SwapchainSettings settings = m_Swapchain->Settings();
        settings.VSync = enabled;
        ApplySwapchainSettings(settings);
    }

    bool Renderer::IsVSync() const
    {
        return m_Swapchain->Settings().VSync;
    }

    void Renderer::SetExtraSwapchainImage(bool enabled)
    {
        SwapchainSettings settings = m_Swapchain->Settings();
        settings.ExtraImage = enabled;
        ApplySwapchainSettings(settings);
    }

    bool Renderer::HasExtraSwapchainImage() const
    {
        return m_Swapchain->Settings().ExtraImage;
    }

    void Renderer::ApplySwapchainSettings(const SwapchainSettings& settings)
    {
        ReleaseSwapchainImages();
        m_Swapchain->ApplySettings(settings);
    }

    void Renderer::ReleaseSwapchainImages()
    {
        // These may point at an image of the window, which is gone once the swapchain is replaced.
        m_CurrentFrameData.Output = nullptr;
        m_SceneOutput = nullptr;
    }

    void Renderer::CreateDefaultTextures()
    {
        // What a material draws with where it has no texture of its own. One pixel each, as red, green, blue and alpha.
        const auto create = [this](GpuTexture& texture, std::array<uint8_t, 4> pixel, const char* name)
        {
            GpuTextureDesc desc;
            desc.Width = 1;
            desc.Height = 1;
            desc.Format = vk::Format::eR8G8B8A8Unorm;
            desc.Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
            desc.Name = name;
            texture = GpuTexture(*m_Context, desc);
            m_Frame->Uploads->UploadTexture(texture, std::as_bytes(std::span<const uint8_t>(pixel)));
            return m_Frame->Shared->AddTexture(texture.View());
        };

        // Black comes first and gets index 0, which is also what ImGui uses for "no texture".
        m_Frame->BlackTextureIndex = create(m_Frame->BlackTexture, { 0, 0, 0, 255 }, "DefaultBlack");
        m_Frame->WhiteTextureIndex = create(m_Frame->WhiteTexture, { 255, 255, 255, 255 }, "DefaultWhite");

        // A normal pointing straight out of the surface, and a roughness and metalness of one half.
        m_Frame->NormalTextureIndex = create(m_Frame->NormalTexture, { 128, 128, 255, 255 }, "DefaultNormal");
        m_Frame->MetallicRoughnessTextureIndex = create(m_Frame->MetallicRoughnessTexture, { 255, 128, 128, 255 }, "DefaultMetallicRoughness");
    }

    void Renderer::CreateSceneTargets(uint32_t width, uint32_t height)
    {
        m_SceneSize = { width, height };

        const auto create = [this, width, height](GpuTexture& texture, vk::Format format, vk::ImageUsageFlags usage, const char* name)
        {
            GpuTextureDesc desc;
            desc.Width = width;
            desc.Height = height;
            desc.Format = format;
            desc.Usage = usage;
            desc.Name = name;

            // The picture before this one is released once the frames that use it are done.
            texture = GpuTexture(*m_Context, desc);
        };

        // Drawn into, read by shaders, copied from for screenshots and picking, and cleared by a copy command.
        const vk::ImageUsageFlags colorUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
        const vk::ImageUsageFlags depthUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

        create(m_Frame->SceneColor, RenderFormats::SceneColor, colorUsage, "SceneColor");
        create(m_Frame->SceneDepth, RenderFormats::SceneDepth, depthUsage, "SceneDepth");
        create(m_Frame->SceneOutput, RenderFormats::Output, colorUsage, "SceneOutput");
        if (m_ShouldCreateIDTarget)
            create(m_Frame->SceneEntityIds, RenderFormats::EntityIds, colorUsage, "SceneEntityIds");

        // Post-processing reads the scene through the texture table, and the editor shows the output through ImGui.
        m_Frame->SceneColorSlot = TextureSlot(*m_Frame->Shared, m_Frame->Shared->AddTexture(m_Frame->SceneColor.View()));

        // The depth pyramid is built from the depth.
        m_Frame->SceneDepthSlot = TextureSlot(*m_Frame->Shared, m_Frame->Shared->AddTexture(m_Frame->SceneDepth.View()));
        m_Frame->SceneOutputSlot = TextureSlot(*m_Frame->Shared, m_Frame->Shared->AddTexture(m_Frame->SceneOutput.View()));
    }

    void Renderer::PrepareDrawCalls()
    {
        {
            // Laying out the draw commands that culling on the GPU fills in, culling the see-through
            // meshes, and bringing what changed in the scene to the GPU.
            ProfileScope cullScope(m_Profiler, "Cull");

            CullCamera camera;
            camera.Planes.FromViewProjection(m_CurrentFrameData.ViewProjection);
            camera.View = m_CurrentFrameData.View;
            camera.Projection = m_CurrentFrameData.Projection;
            Frustum sunFrustum;
            sunFrustum.FromViewProjection(m_CurrentFrameData.LightViewProj);

            const GpuScene::Frame meshes = m_GpuScene->Prepare(camera, sunFrustum, m_CurrentFrameData.CameraPosition);
            m_CurrentFrameData.MeshDraws = meshes.Draws;
            m_CurrentFrameData.Culling = meshes.Culling;
            m_CurrentFrameData.TransparentDraws = meshes.TransparentDraws;
            m_CurrentFrameData.MeshInstanceAddress = meshes.InstanceAddress;
            m_CurrentFrameData.VisibleInstanceAddress = meshes.VisibleAddress;
            m_CurrentFrameData.IndexCount += static_cast<uint32_t>(meshes.TrianglesDrawn * 3);

            // An instance is one submesh of one mesh in the scene. The camera's view and the sun's are
            // counted apart, and a triangle counts once per view, however many passes draw it.
            // What was drawn is decided on the GPU, so those numbers are of a frame a few frames back.
            m_Profiler.SetCounter("InstancesTested", static_cast<double>(meshes.InstancesTested));
            m_Profiler.SetCounter("InstancesDrawn", static_cast<double>(meshes.InstancesDrawn));
            m_Profiler.SetCounter("TrianglesDrawn", static_cast<double>(meshes.TrianglesDrawn));
            m_Profiler.SetCounter("ShadowInstancesDrawn", static_cast<double>(meshes.ShadowInstancesDrawn));
            m_Profiler.SetCounter("ShadowTrianglesDrawn", static_cast<double>(meshes.ShadowTrianglesDrawn));
            m_Profiler.SetCounter("DrawCommands", static_cast<double>(meshes.DrawCommands));
        }

        ProfileScope prepareScope(m_Profiler, "PrepareDraws");
        m_CurrentFrameData.Geometry = m_Frame->Geometry.get();

        // Asked for after the last material of the frame is in: the table moves when it grows.
        m_CurrentFrameData.MaterialAddress = m_Materials->Address();

        std::vector<ParticleInstanceData> allParticleData;
        m_CurrentFrameData.ParticleQueue.clear();

        for (auto& [material, particles] : m_ParticleBatches)
        {
            if (particles.empty())
                continue;

            ParticleBatch batch;
            batch.Material = material;
            batch.StartOffset = (uint32_t)allParticleData.size();
            batch.Count = (uint32_t)particles.size();

            // A material without a texture, or with one that is still loading, draws with plain white.
            batch.AlbedoTexture = m_Frame->WhiteTextureIndex;
            batch.AlbedoSampler = SamplerIndex(SamplerSettings());
            if (material->AlbedoTexture)
            {
                const std::shared_ptr<Texture> albedo = m_Assets.GetAsset<Texture>(material->AlbedoTexture);
                if (albedo && albedo->IsOnGpu())
                {
                    batch.AlbedoTexture = albedo->GetColorTableIndex();
                    batch.AlbedoSampler = albedo->GetSamplerIndex();
                }
            }

            m_CurrentFrameData.ParticleQueue.push_back(batch);
            allParticleData.insert(allParticleData.end(), particles.begin(), particles.end());
        }

        m_Frame->ParticleInstances.Upload<ParticleInstanceData>(*m_Context, *m_Frame->Uploads, allParticleData, vk::BufferUsageFlagBits::eStorageBuffer, "ParticleInstanceBuffer");
        m_CurrentFrameData.ParticleInstanceAddress = m_Frame->ParticleInstances.Buffer.Address();

        // Brings this frame's instances and the scene constants to the GPU before the first pass.
        m_Frame->Uploads->Flush(m_Frame->Commands);
    }

    void Renderer::BeginScene(const glm::mat4& view, const glm::mat4 projection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity, float deltaTime, bool editMode)
    {
        ResetStats();
        m_Stats.FrameTime = deltaTime;

        m_ParticleBatches.clear();
        m_CurrentFrameData.MeshDraws = {};
        m_CurrentFrameData.TransparentDraws = {};
        m_CurrentFrameData.ParticleQueue.clear();

        glm::vec3 center = cameraPosition;

        glm::vec3 lightDirNorm = glm::normalize(lightDir);
        glm::vec3 lightPos = center - lightDirNorm * 100.0f; // Move back enough to not clip near objects

        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(lightDirNorm, up)) > 0.99f)
            up = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::mat4 lightView = glm::lookAt(lightPos, center, up);

        float orthoSize = 100.0f;
        float zNear = 0.1f;
        float zFar = 1000.0f;

        glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, zNear, zFar);

        glm::mat4 lightViewProj = lightProj * lightView;

        // Shadow Map Stabilization
        // We need to snap the projection to the nearest texel in Light Space to avoid shimmering
        float shadowMapRes = (float)m_ShadowMapResolution;

        // Project a reference point (Origin) to Shadow Map NDC Space
        glm::vec4 shadowOrigin = lightViewProj * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin /= shadowOrigin.w;

        // NDC is [-1, 1] for XY. Resolution is [0, Res].
        float texelSizeNDC = 2.0f / shadowMapRes;

        // Snap to grid
        float snappedX = round(shadowOrigin.x / texelSizeNDC) * texelSizeNDC;
        float snappedY = round(shadowOrigin.y / texelSizeNDC) * texelSizeNDC;

        // Calculate offset
        float dX = snappedX - shadowOrigin.x;
        float dY = snappedY - shadowOrigin.y;

        // Apply offset to the Projection Matrix
        lightViewProj[3][0] += dX;
        lightViewProj[3][1] += dY;

        m_CurrentFrameData.View = view;
        m_CurrentFrameData.Projection = projection;
        m_CurrentFrameData.ViewProjection = projection * view;
        m_CurrentFrameData.CameraPosition = cameraPosition;
        m_CurrentFrameData.LightDirection = lightDir;
        m_CurrentFrameData.LightColor = lightColor;
        m_CurrentFrameData.LightIntensity = lightIntensity;
        m_CurrentFrameData.LightViewProj = lightViewProj;
        m_CurrentFrameData.ShowGrid = editMode && m_ShowGrid;
        if (!m_SceneSize)
        {
            auto [w, h] = GetViewportSize();
            CreateSceneTargets(w, h);
        }

        m_CurrentFrameData.SceneColor = &m_Frame->SceneColor;
        m_CurrentFrameData.SceneDepth = &m_Frame->SceneDepth;
        m_CurrentFrameData.SceneEntityIds = m_Frame->SceneEntityIds ? &m_Frame->SceneEntityIds : nullptr;

        // Negating the second row turns the Y of every position a shader computes with the matrix.
        const auto toVulkanClip = [](glm::mat4 matrix)
        {
            for (int column = 0; column < 4; ++column)
                matrix[column][1] = -matrix[column][1];
            return matrix;
        };

        // The camera's depth is stored the other way round: 1 at the near plane and smaller with distance.
        // Depth is a 32-bit float, which is finest near 0, and a perspective squeezes almost everything far away
        // into a tiny range of depth. Putting the far end at 0 lets the two cancel out, so that surfaces
        // kilometres away still get distinct depths. A perspective then needs no far plane at all on the GPU.
        // The CPU keeps the camera's own projection, far plane included, and culls with it.
        const auto toReversedDepth = [](glm::mat4 cameraProjection)
        {
            const bool isPerspective = cameraProjection[2][3] != 0.0f;
            if (isPerspective)
            {
                const float nearPlane = cameraProjection[3][2] / cameraProjection[2][2];
                cameraProjection[2][2] = 0.0f;
                cameraProjection[3][2] = nearPlane;
            }
            else
            {
                // The new depth is one minus the old one.
                for (int column = 0; column < 4; ++column)
                    cameraProjection[column][2] = cameraProjection[column][3] - cameraProjection[column][2];
            }
            return cameraProjection;
        };
        m_CurrentFrameData.ClipViewProjection = toVulkanClip(toReversedDepth(projection) * view);

        // The shadow map keeps the usual depth: its view has no perspective, so there is nothing to gain.
        m_CurrentFrameData.ClipLightViewProjection = toVulkanClip(lightViewProj);

        SceneData sceneData;
        sceneData.ViewProjectionMatrix = m_CurrentFrameData.ClipViewProjection;
        sceneData.LightViewProjection = m_CurrentFrameData.ClipLightViewProjection;
        sceneData.CameraPosition = glm::vec4(cameraPosition, 1.0f);
        sceneData.LightDirection = glm::vec4(lightDir, lightIntensity);
        sceneData.LightColor = glm::vec4(lightColor, 1.0f);
        m_Frame->Uploads->Upload(m_Frame->SceneConstants.Handle(), 0, std::as_bytes(std::span<const SceneData>(&sceneData, 1)));

        // The scene starts as a dark grey with everything farthest away, which is depth 0, and with no entity anywhere.
        const vk::CommandBuffer commands = m_Frame->Commands;
        m_Frame->SceneColor.ClearColor(commands, vk::ClearColorValue(0.05f, 0.05f, 0.05f, 1.0f));
        m_Frame->SceneDepth.ClearDepth(commands, 0.0f);
        if (m_Frame->SceneEntityIds)
            m_Frame->SceneEntityIds.ClearColor(commands, vk::ClearColorValue(std::array<int32_t, 4>{ -1, 0, 0, 0 }));
    }

    void Renderer::RenderScene(const ExtractedScene& scene)
    {
        BeginScene(scene.View, scene.Projection, scene.CameraPosition, scene.SunDirection, scene.SunColor, scene.SunIntensity, scene.DeltaTime, scene.IsEditorView);

        for (const ExtractedParticles& particles : scene.Particles)
            SubmitParticles(particles.Material.get(), particles.Particles);

        m_CurrentFrameData.DebugLines = scene.DebugLines;
        m_CurrentFrameData.Canvases = scene.Canvases;

        EndScene();

        // The lists belong to the caller or last for one frame, and may be gone after this call.
        m_CurrentFrameData.DebugLines = {};
        m_CurrentFrameData.Canvases = {};
        m_CurrentFrameData.TransparentDraws = {};
    }

    TextureSlot Renderer::AddToTextureTable(const GpuTexture& texture)
    {
        return TextureSlot(*m_Frame->Shared, m_Frame->Shared->AddTexture(texture.View()));
    }

    TextureSlot Renderer::AddColorToTextureTable(const GpuTexture& texture)
    {
        if (!texture.SrgbView())
            return TextureSlot();
        return TextureSlot(*m_Frame->Shared, m_Frame->Shared->AddTexture(texture.SrgbView()));
    }

    uint32_t Renderer::SamplerIndex(const SamplerSettings& settings)
    {
        return m_Frame->Shared->SamplerIndex(SamplerCache::Get()->GetSampler(settings));
    }

    void Renderer::SubmitParticles(Material* material, const std::vector<ParticleInstanceData>& particles)
    {
        if (!material || particles.empty())
            return;

        auto& batch = m_ParticleBatches[material];
        batch.insert(batch.end(), particles.begin(), particles.end());
    }

    void Renderer::EndScene()
    {
        PrepareDrawCalls();

        // The editor shows the finished picture inside its viewport panel. A game shows it in the window.
        m_CurrentFrameData.Output = m_ShouldCreateIDTarget ? &m_Frame->SceneOutput : &m_Swapchain->Image();
        m_CurrentFrameData.SceneColorIndex = m_Frame->SceneColorSlot.Index();
        m_CurrentFrameData.SceneDepthIndex = m_Frame->SceneDepthSlot.Index();
        m_CurrentFrameData.DepthPyramid = {};
        m_CurrentFrameData.BloomTextureIndex = m_Frame->BlackTextureIndex;
        m_CurrentFrameData.Bloom = m_BloomSettings;
        m_CurrentFrameData.FXAAEnabled = m_FXAAEnabled;

        m_Graph->Execute(m_RenderContext, m_CurrentFrameData);
        m_GpuScene->CopyCommandsForCounters(m_Frame->Commands);
        CopyRequestedEntityId();

        m_Stats.DrawCalls = m_CurrentFrameData.DrawCalls;
        m_Stats.IndexCount = m_CurrentFrameData.IndexCount;
        m_Profiler.SetCounter("DrawCalls", m_Stats.DrawCalls);
        m_Profiler.SetCounter("Triangles", m_Stats.IndexCount / 3);

        m_SceneOutput = m_CurrentFrameData.Output;
    }

    bool Renderer::BeginFrame()
    {
        // Acquiring an image may replace the swapchain.
        ReleaseSwapchainImages();

        {
            // Waiting here is where the CPU is held back by the GPU or, with VSync, by the display.
            ProfileScope waitScope(m_Profiler, "WaitForGpu");

            const vk::Device device = m_Context->Device();
            if (device.waitForFences(1, &m_Frame->InFlight[m_CurrentFrame], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
                LX_CORE_ERROR("Failed to wait for fence!");

            if (!m_Swapchain->AcquireImage(m_Frame->ImageAvailable[m_CurrentFrame]))
                return false;

            // Reset only now: a frame that is skipped submits nothing that would signal the fence again.
            device.resetFences(1, &m_Frame->InFlight[m_CurrentFrame]);
        }

        m_Context->Deletions().BeginFrame();
        m_Frame->Uploads->BeginFrame();
        m_Frame->Geometry->BeginFrame();
        m_Materials->BeginFrame();

        // Resetting the pool is allowed now: the fence above says the GPU is done with this frame's last commands.
        m_Context->Device().resetCommandPool(m_Frame->CommandPools[m_CurrentFrame]);
        m_Frame->Commands = m_Frame->CommandBuffers[m_CurrentFrame];
        m_Frame->Commands.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        m_RenderContext.Commands = m_Frame->Commands;
        m_Frame->Timers->BeginFrame(m_Profiler);
        m_Frame->Timers->Begin(m_Frame->Commands, "Frame");
        return true;
    }

    void Renderer::EndFrame()
    {
        const vk::CommandBuffer commands = m_Frame->Commands;
        GpuTexture& window = m_Swapchain->Image();

        if (m_SceneOutput)
        {
            if (!m_ScreenshotRequests.empty())
                CopyFrameForScreenshots(*m_SceneOutput);
        }
        else
        {
            // No scene was drawn, for example because it has no camera. Nothing else writes the whole window,
            // and what a swapchain image holds before it is written is undefined.
            window.ClearColor(commands, vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));
        }

        DrawImGui(window);

        // From here the picture belongs to the window system until it is acquired again.
        window.Transition(commands, vk::ImageLayout::ePresentSrcKHR);

        m_Frame->Timers->End(commands, "Frame");

        // Submitting and presenting.
        ProfileScope presentScope(m_Profiler, "Present");

        commands.end();

        // The frame's commands wait for the window's image, signal when the image is ready to be shown,
        // and signal the fence that the next use of this frame slot waits for. None of the frame runs
        // before the image is there, because its very first use may be a clear or a change of layout.
        const vk::Semaphore imageAvailable = m_Frame->ImageAvailable[m_CurrentFrame];
        const vk::Semaphore renderFinished = m_Swapchain->RenderFinished();
        const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAllCommands;
        const vk::SubmitInfo submitInfo(imageAvailable, waitStage, commands, renderFinished);
        m_Context->GraphicsQueue().submit(submitInfo, m_Frame->InFlight[m_CurrentFrame]);

        m_Swapchain->Present();
        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        m_ImGui->RenderOtherWindows();

        ReportMemoryUse();
    }

    void Renderer::DrawImGui(GpuTexture& window)
    {
        ProfileScope scope(m_Profiler, "ImGuiDraw");
        const vk::CommandBuffer commands = m_Frame->Commands;
        m_Frame->Timers->Begin(commands, "ImGui");

        // The editor shows the finished scene picture in a panel, so that one has to be readable.
        window.Transition(commands, vk::ImageLayout::eColorAttachmentOptimal);
        if (m_ShouldCreateIDTarget && m_Frame->SceneOutput)
            m_Frame->SceneOutput.Transition(commands, vk::ImageLayout::eShaderReadOnlyOptimal);

        // A frame without a scene has no pass that would bring pending uploads to the GPU,
        // and ImGui may be about to show a texture that was just loaded.
        m_Frame->Uploads->Flush(commands);

        RenderingTargets targets;
        targets.Color = window.View();
        targets.Extent = vk::Extent2D(window.Desc().Width, window.Desc().Height);
        BeginRendering(commands, targets);
        m_ImGui->Render(commands);
        commands.endRendering();

        m_Frame->Timers->End(commands, "ImGui");
    }

    void Renderer::BeginImGuiFrame()
    {
        m_ImGui->NewFrame();
    }

    void Renderer::ShutdownImGui()
    {
        m_Context->WaitIdle();
        m_ImGui.reset();
    }

    uint64_t Renderer::GetViewportImGuiTexture() const
    {
        return m_Frame->SceneOutputSlot ? m_ImGui->TextureId(m_Frame->SceneOutputSlot.Index()) : 0;
    }

    uint64_t Renderer::GetImGuiTexture(uint32_t tableIndex) const
    {
        return m_ImGui->TextureId(tableIndex);
    }

    void Renderer::RequestScreenshot(uint64_t tag)
    {
        m_ScreenshotRequests.push_back(tag);
    }

    void Renderer::CopyFrameForScreenshots(GpuTexture& frame)
    {
        const vk::CommandBuffer commands = m_Frame->Commands;
        const uint32_t width = frame.Desc().Width;
        const uint32_t height = frame.Desc().Height;

        frame.Transition(commands, vk::ImageLayout::eTransferSrcOptimal);

        for (uint64_t tag : m_ScreenshotRequests)
        {
            GpuBufferDesc desc;
            desc.Size = static_cast<uint64_t>(width) * height * 4;
            desc.Usage = vk::BufferUsageFlagBits::eTransferDst;
            desc.CpuReadable = true;
            desc.Name = "Screenshot";

            FrameResources::PendingScreenshot& pending = m_Frame->PendingScreenshots.emplace_back();
            pending.Tag = tag;
            pending.Frame = m_Profiler.CurrentFrame();
            pending.Width = width;
            pending.Height = height;
            pending.Pixels = GpuBuffer(*m_Context, desc);

            vk::BufferImageCopy region;
            region.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
            region.imageExtent = vk::Extent3D(width, height, 1);
            commands.copyImageToBuffer(frame.Image(), vk::ImageLayout::eTransferSrcOptimal, pending.Pixels.Handle(), region);
        }

        m_ScreenshotRequests.clear();
    }

    std::vector<Screenshot> Renderer::TakeScreenshots(bool waitForAll)
    {
        auto& pendingScreenshots = m_Frame->PendingScreenshots;
        if (waitForAll && !pendingScreenshots.empty())
            m_Context->WaitIdle();

        std::vector<Screenshot> finished;
        const uint64_t currentFrame = m_Profiler.CurrentFrame();

        for (auto pending = pendingScreenshots.begin(); pending != pendingScreenshots.end();)
        {
            if (!waitForAll && currentFrame < pending->Frame + ReadbackDelay)
            {
                ++pending;
                continue;
            }

            Screenshot screenshot;
            screenshot.Tag = pending->Tag;
            screenshot.Width = pending->Width;
            screenshot.Height = pending->Height;
            screenshot.Rgba.resize(static_cast<size_t>(pending->Width) * pending->Height * 4);

            // The frame is BGRA. Its alpha channel does not describe coverage, so the picture is made opaque.
            const auto* source = static_cast<const uint8_t*>(pending->Pixels.Data());
            for (size_t pixel = 0; pixel < screenshot.Rgba.size(); pixel += 4)
            {
                screenshot.Rgba[pixel + 0] = source[pixel + 2];
                screenshot.Rgba[pixel + 1] = source[pixel + 1];
                screenshot.Rgba[pixel + 2] = source[pixel + 0];
                screenshot.Rgba[pixel + 3] = 255;
            }

            finished.push_back(std::move(screenshot));
            pending = pendingScreenshots.erase(pending);
        }

        return finished;
    }

    void Renderer::FlushGpuTimers()
    {
        m_Context->WaitIdle();
        m_Frame->Timers->Flush(m_Profiler);
    }

    std::string Renderer::GetAdapterName() const
    {
        return m_Context->AdapterName();
    }

    void Renderer::ReportMemoryUse()
    {
        if (const std::optional<uint64_t> usedBytes = m_Context->VideoMemoryInUse())
            m_Profiler.SetCounter("VideoMemoryMB", static_cast<double>(*usedBytes) / (1024.0 * 1024.0));

        // What the vertices and indices of all loaded meshes take up.
        m_Profiler.SetCounter("GeometryMB", static_cast<double>(m_Frame->Geometry->UsedBytes()) / (1024.0 * 1024.0));
    }

    MeshGeometry Renderer::CreateMeshGeometry(std::span<const PackedVertex> vertices, std::span<const uint32_t> indices)
    {
        return m_Frame->Geometry->Add(std::as_bytes(vertices), indices);
    }

    void Renderer::OnResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        ReleaseSwapchainImages();
        m_Swapchain->Resize(width, height);
    }

    void Renderer::EnsureEditorViewport(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        // No waiting for the GPU: the old pictures are released once the frames that use them are done.
        if (!m_SceneSize || m_SceneSize->first != width || m_SceneSize->second != height)
            CreateSceneTargets(width, height);
    }

    std::pair<uint32_t, uint32_t> Renderer::GetViewportSize() const
    {
        if (m_SceneSize)
            return *m_SceneSize;

        return { m_Swapchain->Width(), m_Swapchain->Height() };
    }
}
