#include "Renderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "ShaderUtils.h"
#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

#include "ExtractedScene.h"
#include "Frustum.h"
#include "Graph/RenderGraph.h"
#include "Passes/CompositePass.h"
#include "Lynx/UI/Rendering/UIPass.h"
#include "GpuTimers.h"
#include "GraphicsContext.h"
#include "SamplerCache.h"
#include "Swapchain.h"
#include "Vulkan/GpuBuffer.h"
#include "Vulkan/GpuTexture.h"
#include "Vulkan/SharedBindings.h"
#include "Vulkan/UploadQueue.h"
#include "Lynx/Profiling/FrameProfiler.h"
#include "Passes/DebugPass.h"
#include "Passes/ForwardPass.h"
#include "Lynx/Asset/AssetManager.h"
#include "Passes/BloomPass.h"
#include "Passes/GridPass.h"
#include "RenderFormats.h"
#include "Passes/ShadowPass.h"
#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"
#include "Passes/DepthPass.h"
#include "Passes/ParticlePass.h"

#include <shaderc/shaderc.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "nvrhi/utils.h"

#include "Lynx/ImGui/imgui_nvrhi.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>


namespace Lynx
{
    namespace Helpers
    {
        static nvrhi::Format TextureFormatToNvrhi(TextureFormat format, bool sRGB)
        {
            switch (format)
            {
                case TextureFormat::RGBA8:
                    return sRGB ? nvrhi::Format::SRGBA8_UNORM : nvrhi::Format::RGBA8_UNORM;
                case TextureFormat::RG16F:
                    return nvrhi::Format::RG16_FLOAT;
                case TextureFormat::RG32F:
                    return nvrhi::Format::RG32_FLOAT;
                case TextureFormat::R32I:
                    return sRGB ? nvrhi::Format::R32_SINT : nvrhi::Format::R32_UINT;
                case TextureFormat::R8:
                    return nvrhi::Format::R8_UNORM;
                case TextureFormat::Depth32:
                    return nvrhi::Format::D32;
                case TextureFormat::Depth24Stencil8:
                    return nvrhi::Format::D24S8;
            }

            return nvrhi::Format::UNKNOWN;
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
        // An asset can be loaded in the middle of a frame, while the frame's command list is open.
        // NVRHI allows only one immediate command list to be open at a time, so uploads use one that is not immediate.
        nvrhi::CommandListHandle CreateUploadCommandList(nvrhi::IDevice* device)
        {
            return device->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
        }

        vk::CommandBuffer NativeCommandBuffer(nvrhi::ICommandList* commandList)
        {
            return vk::CommandBuffer(static_cast<VkCommandBuffer>(commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)));
        }

        // A screenshot is read this many frames after it was copied, when the GPU has certainly finished the copy.
        constexpr uint64_t ScreenshotReadDelay = 3;

        double MillisecondsSince(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    }

    // What a frame needs from Vulkan directly. Two frames can be under way at once:
    // the CPU records one while the GPU still draws the one before.
    struct Renderer::FrameResources
    {
        std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> ImageAvailable;
        std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> InFlight;
        std::unique_ptr<GpuTimers> Timers;
        std::unique_ptr<UploadQueue> Uploads;
        std::unique_ptr<SharedBindings> Shared;

        // Where the texture table has the scene's HDR picture and the black default texture.
        std::optional<uint32_t> SceneColorIndex;
        uint32_t BlackTextureIndex = 0;
        uint32_t WhiteTextureIndex = 0;

        // Rewritten every frame with one entry per drawn instance. The NVRHI passes read it through
        // the wrapper, which is a second handle to the same Vulkan buffer.
        struct InstanceStorage
        {
            GpuBuffer Buffer;
            nvrhi::BufferHandle Wrapper;

            // Makes sure the buffer can hold the instances, then queues them for upload.
            template<typename Instance>
            void Upload(GraphicsContext& context, UploadQueue& uploads, std::span<const Instance> instances, const char* name)
            {
                const uint64_t requiredSize = instances.size_bytes();
                if (requiredSize == 0)
                    return;

                if (Buffer.Size() < requiredSize)
                {
                    // Half as much again as needed, so a scene that grows a little each frame does not reallocate each frame.
                    GpuBufferDesc desc;
                    desc.Size = requiredSize + requiredSize / 2;
                    desc.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
                    desc.Name = name;
                    Buffer = GpuBuffer(context, desc);

                    // The upload queue makes the buffer readable after every write, so NVRHI may treat it
                    // as a shader resource at all times and never has to change its state.
                    nvrhi::BufferDesc wrapperDesc;
                    wrapperDesc.byteSize = desc.Size;
                    wrapperDesc.structStride = sizeof(Instance);
                    wrapperDesc.debugName = name;
                    wrapperDesc.initialState = nvrhi::ResourceStates::ShaderResource;
                    wrapperDesc.keepInitialState = true;
                    Wrapper = context.Nvrhi()->createHandleForNativeBuffer(nvrhi::ObjectTypes::VK_Buffer, nvrhi::Object(static_cast<VkBuffer>(Buffer.Handle())), wrapperDesc);
                }

                uploads.Upload(Buffer.Handle(), 0, std::as_bytes(instances));
            }
        };
        InstanceStorage MeshInstances;
        InstanceStorage ParticleInstances;

        // The pictures a scene is drawn into. The handles in RenderTarget are NVRHI's view of these.
        GpuTexture SceneColor;
        GpuTexture SceneDepth;
        GpuTexture SceneOutput;
        GpuTexture SceneEntityIds;
    };

    Renderer::Renderer(GLFWwindow* window, FrameProfiler& profiler, AssetManager& assets, bool initIdTarget)
        : m_ShouldCreateIDTarget(initIdTarget), m_Profiler(profiler), m_Assets(assets)
    {
        m_Context = std::make_unique<GraphicsContext>(window);
        m_NvrhiDevice = m_Context->Nvrhi();

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
        }
        m_Frame->Timers = std::make_unique<GpuTimers>(m_Context->PhysicalDevice(), m_Context->Device(), m_Context->GraphicsQueueFamily());
        m_Frame->Uploads = std::make_unique<UploadQueue>(*m_Context);
        m_Graph = std::make_unique<RenderGraph>(m_Profiler, *m_Frame->Timers);

        m_CommandList = m_NvrhiDevice->createCommandList();
        CreateDefaultTextures();
        InitBuffers();

        // The scene constants are still a buffer of NVRHI's. The shared bindings point at the Vulkan buffer behind it.
        const vk::Buffer sceneConstants(static_cast<VkBuffer>(m_GlobalCB->getNativeObject(nvrhi::ObjectTypes::VK_Buffer).pointer));
        m_Frame->Shared = std::make_unique<SharedBindings>(*m_Context, sceneConstants);
        m_Frame->BlackTextureIndex = m_Frame->Shared->AddTexture(vk::ImageView(static_cast<VkImageView>(m_BlackTex->getNativeView(nvrhi::ObjectTypes::VK_ImageView).pointer)));
        m_Frame->WhiteTextureIndex = m_Frame->Shared->AddTexture(vk::ImageView(static_cast<VkImageView>(m_WhiteTex->getNativeView(nvrhi::ObjectTypes::VK_ImageView).pointer)));
    }

    Renderer::~Renderer()
    {
        LX_CORE_INFO("Renderer shutting down...");

        m_Context->WaitIdle();

        // Texture assets are gone by now. Their pictures were retired and hold NVRHI textures.
        m_Context->Deletions().Flush();
        m_NvrhiDevice->runGarbageCollection();

        // Everything made from the device goes before the device does.
        ReleaseSwapchainImages();
        m_Frame->MeshInstances = {};
        m_Frame->ParticleInstances = {};
        m_Frame->Uploads.reset();
        m_PendingScreenshots.clear();
        m_Graph.reset();
        m_Frame->Shared.reset();
        m_MipMapGenPass.reset();
        m_RenderContext = RenderContext();
        m_CurrentFrameData = RenderData();
        m_OpaqueBatches.clear();
        m_StageBuffer = nullptr;

        SamplerCache::Shutdown();
        m_WhiteTex = nullptr;
        m_NormalTex = nullptr;
        m_BlackTex = nullptr;
        m_MetallicRoughnessTex = nullptr;
        m_GlobalCB = nullptr;
        m_CommandList = nullptr;
        m_SceneTarget.reset();
        m_ImGuiBackend.reset();
        m_NvrhiDevice = nullptr;

        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            m_Context->Device().destroySemaphore(m_Frame->ImageAvailable[frame]);
            m_Context->Device().destroyFence(m_Frame->InFlight[frame]);
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
        m_RenderContext.Device = m_NvrhiDevice;
        m_RenderContext.CommandList = m_CommandList;
        m_RenderContext.GlobalConstantBuffer = m_GlobalCB;
        m_RenderContext.WhiteTexture = m_WhiteTex;
        m_RenderContext.BlackTexture = m_BlackTex;
        m_RenderContext.NormalTexture = m_NormalTex;
        m_RenderContext.MetallicRoughnessTexture = m_MetallicRoughnessTex;

        // TODO: Make sure this is up-to-date...
        nvrhi::FramebufferInfo fbInfo;
        fbInfo.addColorFormat(nvrhi::Format::RGBA16_FLOAT);
        if (m_ShouldCreateIDTarget) fbInfo.addColorFormat(nvrhi::Format::R32_SINT);
        fbInfo.setDepthFormat(nvrhi::Format::D32);
        m_RenderContext.PresentationFramebufferInfo = fbInfo;
        nvrhi::FramebufferInfo finalfbInfo;
        finalfbInfo.addColorFormat(nvrhi::Format::BGRA8_UNORM);
        m_RenderContext.FinalFramebufferInfo = finalfbInfo;

        SamplerCache::Init(m_NvrhiDevice, m_MaxAnisotropy);

        // The frame, start to finish. The scene passes draw the HDR picture, bloom reads it and composite
        // turns both into the output. What is not part of the scene is drawn on top of the output, so that
        // post-processing does not touch it: the grid and debug lines, still hidden by the scene's depth, then the UI.
        m_Graph->Add("Shadow", std::make_unique<ShadowPass>(m_ShadowMapResolution));
        m_Graph->Add("Depth", std::make_unique<DepthPass>());
        m_Graph->Add("Forward", std::make_unique<ForwardPass>());
        m_Graph->Add("Particles", std::make_unique<ParticlePass>());
        m_Graph->Add("Bloom", std::make_unique<BloomPass>());
        m_Graph->Add("Composite", std::make_unique<CompositePass>());
        if (m_ShouldCreateIDTarget)
            m_Graph->Add("Grid", std::make_unique<GridPass>());
        m_Graph->Add("Debug", std::make_unique<DebugPass>());
        m_Graph->Add("UI", std::make_unique<UIPass>(), [this]() { return m_ShowUI; });
        m_Graph->Init(m_RenderContext);

        m_ImGuiBackend= std::make_unique<ImGui_NVRHI>();
        m_ImGuiBackend->init(m_NvrhiDevice);

        m_MipMapGenPass = std::make_unique<MipMapBlitPass>();
        m_MipMapGenPass->Init(m_NvrhiDevice);

        LX_CORE_INFO("Renderer initialized successfully (Pipeline loaded).");
    }

    nvrhi::TextureHandle Renderer::CreateTexture(const TextureSpecification& specification, unsigned char* data)
    {
        uint32_t mipLevels = 1;
        if (specification.GenerateMips)
        {
            mipLevels = (uint32_t)(std::floor(std::log2(std::max(specification.Width, specification.Height))) + 1);
        }
        
        auto desc = nvrhi::TextureDesc()
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setWidth(specification.Width)
            .setHeight(specification.Height)
            .setFormat(Helpers::TextureFormatToNvrhi(specification.Format, specification.IsSRGB))
            .setDebugName(specification.DebugName)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource)
            .setMipLevels(mipLevels)
            .setIsUAV(false)
            .setIsRenderTarget(specification.GenerateMips);

        auto result = m_NvrhiDevice->createTexture(desc);
        if (!result)
        {
            LX_CORE_ERROR("Failed to create NVRHI texture from data");
            return nullptr;
        }

        if (data)
        {
            uint32_t bytesPerPixel = Helpers::GetTextureFormatByteSize(specification.Format);

            size_t rowPitch = specification.Width * bytesPerPixel;
            size_t depthPitch = 0;

            // TODO: Batch this!
            auto cmdList = CreateUploadCommandList(m_NvrhiDevice);
            cmdList->open();
            cmdList->writeTexture(result, 0, 0, data, rowPitch, depthPitch);

            if (specification.GenerateMips)
            {
                m_MipMapGenPass->Generate(cmdList, result);
            }
            
            cmdList->close();
            m_NvrhiDevice->executeCommandList(cmdList);
        }
        
        return result;
    }

    int Renderer::ReadIdFromBuffer(uint32_t x, uint32_t y)
    {
        if (!m_SceneTarget || !m_SceneTarget->IdBuffer)
            return -1;

        if (!m_StageBuffer)
        {
            nvrhi::TextureDesc desc = nvrhi::TextureDesc()
            .setWidth(1)
            .setHeight(1)
            .setFormat(nvrhi::Format::R32_SINT)
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setKeepInitialState(true)
            .setDebugName("ID_Stage");
            m_StageBuffer = m_NvrhiDevice->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
        }

        m_CommandList->open();

        // Define the source region (the pixel under mouse)
        nvrhi::TextureSlice srcSlice;
        srcSlice.x = x;
        srcSlice.y = y;
        srcSlice.z = 0;
        srcSlice.width = 1;
        srcSlice.height = 1;
        srcSlice.depth = 1;
        srcSlice.mipLevel = 0;
        srcSlice.arraySlice = 0;

        // Define dest region (0,0 in staging buffer)
        nvrhi::TextureSlice dstSlice;
        dstSlice.x = 0;
        dstSlice.y = 0;
        dstSlice.z = 0;
        dstSlice.width = 1;
        dstSlice.height = 1;
        dstSlice.depth = 1;
        dstSlice.mipLevel = 0;
        dstSlice.arraySlice = 0;

        m_CommandList->copyTexture(m_StageBuffer, dstSlice, m_SceneTarget->IdBuffer, srcSlice);
        m_CommandList->close();
        m_NvrhiDevice->executeCommandList(m_CommandList);

        // 3. Sync (Wait for copy to finish)
        // TODO: In a real engine, you might want to delay reading by 1-2 frames to avoid stalling.
        // But for an editor tool, a stall is acceptable.
        m_NvrhiDevice->waitForIdle();

        size_t rowPitch;
        void* data = m_NvrhiDevice->mapStagingTexture(m_StageBuffer, dstSlice, nvrhi::CpuAccessMode::Read, &rowPitch);
        
        int entityID = -1;
        if (data)
        {
            entityID = *(int*)data;
            m_NvrhiDevice->unmapStagingTexture(m_StageBuffer);
        }

        return entityID;
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
        // These may refer to an image of the window. A swapchain cannot be replaced or destroyed while one is held.
        m_CurrentFrameData.TargetFramebuffer = nullptr;
        m_CurrentFrameData.OutputFramebuffer = nullptr;
        m_SceneOutput = nullptr;
    }

    void Renderer::CreateDefaultTextures()
    {
        nvrhi::TextureDesc defaultTexDesc;
        defaultTexDesc.width = 1;
        defaultTexDesc.height = 1;
        defaultTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultTexDesc.debugName = "DefaultWhite";
        defaultTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultTexDesc.keepInitialState = true;
        m_WhiteTex = m_NvrhiDevice->createTexture(defaultTexDesc);

        nvrhi::TextureDesc defaultNormalTexDesc;
        defaultNormalTexDesc.width = 1;
        defaultNormalTexDesc.height = 1;
        defaultNormalTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultNormalTexDesc.debugName = "DefaultNormal";
        defaultNormalTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultNormalTexDesc.keepInitialState = true;
        m_NormalTex = m_NvrhiDevice->createTexture(defaultNormalTexDesc);

        nvrhi::TextureDesc defaultMrTexDesc;
        defaultMrTexDesc.width = 1;
        defaultMrTexDesc.height = 1;
        defaultMrTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultMrTexDesc.debugName = "DefaultMetallicRoughness";
        defaultMrTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultMrTexDesc.keepInitialState = true;
        m_MetallicRoughnessTex = m_NvrhiDevice->createTexture(defaultMrTexDesc);
        
        nvrhi::TextureDesc defaultBlackTexDesc;
        defaultBlackTexDesc.width = 1;
        defaultBlackTexDesc.height = 1;
        defaultBlackTexDesc.format = nvrhi::Format::RGBA8_UNORM;
        defaultBlackTexDesc.debugName = "DefaultBlack";
        defaultBlackTexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        defaultBlackTexDesc.keepInitialState = true;
        m_BlackTex = m_NvrhiDevice->createTexture(defaultBlackTexDesc);

        uint32_t whitePixel = 0xFFFFFFFF;
        uint32_t normalPixel = 0xFFFF8080;
        uint32_t mrPixel = 0xFF8080FF;
        uint32_t blackPixel = 0xFF000000;
        m_CommandList->open();
        m_CommandList->writeTexture(m_WhiteTex, 0, 0, &whitePixel, 4);
        m_CommandList->writeTexture(m_NormalTex, 0, 0, &normalPixel, 4);
        m_CommandList->writeTexture(m_BlackTex, 0, 0, &blackPixel, 4);
        m_CommandList->writeTexture(m_MetallicRoughnessTex, 0, 0, &mrPixel, 4);
        m_CommandList->close();
        m_NvrhiDevice->executeCommandList(m_CommandList);
    }
    
    void Renderer::InitBuffers()
    {
        nvrhi::BufferDesc cbDesc;
        cbDesc.byteSize = sizeof(SceneData);
        cbDesc.isConstantBuffer = true;
        cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        cbDesc.keepInitialState = true;
        m_GlobalCB = m_NvrhiDevice->createBuffer(cbDesc);

        if (!m_GlobalCB)
        {
            LX_CORE_ERROR("Failed to create Constant Buffer!");
        }
    }
    
    void Renderer::CreateRenderTarget(RenderTarget& target, uint32_t width, uint32_t height)
    {
        target.Width = width;
        target.Height = height;

        // The renderer owns the picture. NVRHI gets a handle to the same picture, which its passes draw with
        // and through which it keeps track of the picture's layout. The first use moves it out of its undefined
        // starting layout, as NVRHI does for a handle whose state it has not seen yet.
        const auto createTarget = [this, width, height](GpuTexture& texture, vk::Format format, vk::ImageUsageFlags usage, const nvrhi::TextureDesc& desc)
        {
            GpuTextureDesc textureDesc;
            textureDesc.Width = width;
            textureDesc.Height = height;
            textureDesc.Format = format;
            textureDesc.Usage = usage;
            textureDesc.Name = desc.debugName.c_str();

            texture = GpuTexture(*m_Context, textureDesc);
            return m_NvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(static_cast<VkImage>(texture.Image())), desc);
        };

        // Drawn into, read by shaders, copied from for screenshots and picking, and cleared by a copy command.
        const vk::ImageUsageFlags colorUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
        const vk::ImageUsageFlags depthUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

        auto colorDesc = nvrhi::TextureDesc()
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::RGBA16_FLOAT)
            .setIsRenderTarget(true)
            .setDebugName("SceneHDR")
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true);
        target.Color = createTarget(m_Frame->SceneColor, RenderFormats::SceneColor, colorUsage, colorDesc);

        // Post-processing reads the scene through the texture table. The picture before this one leaves it.
        if (m_Frame->SceneColorIndex)
            m_Frame->Shared->ReleaseTexture(*m_Frame->SceneColorIndex);
        m_Frame->SceneColorIndex = m_Frame->Shared->AddTexture(m_Frame->SceneColor.View());

        auto depthDesc = nvrhi::TextureDesc()
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::D32)
            .setIsRenderTarget(true)
            .setDebugName("SceneDepth")
            .setInitialState(nvrhi::ResourceStates::DepthWrite)
            .setKeepInitialState(true);
        target.Depth = createTarget(m_Frame->SceneDepth, RenderFormats::SceneDepth, depthUsage, depthDesc);

        auto outputDesc = nvrhi::TextureDesc()
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::BGRA8_UNORM)
            .setIsRenderTarget(true)
            .setDebugName("SceneLDR")
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true);
        target.Output = createTarget(m_Frame->SceneOutput, RenderFormats::Output, colorUsage, outputDesc);

        if (m_ShouldCreateIDTarget)
        {
            auto idDesc = nvrhi::TextureDesc()
                .setWidth(width)
                .setHeight(height)
                .setFormat(nvrhi::Format::R32_SINT)
                .setIsRenderTarget(true)
                .setDebugName("OffscreenId")
                .setInitialState(nvrhi::ResourceStates::RenderTarget)
                .setKeepInitialState(true)
                .setClearValue(nvrhi::Color(-1.0f, 0.0f, 0.0f, 0.0f));
            target.IdBuffer = createTarget(m_Frame->SceneEntityIds, vk::Format::eR32Sint, colorUsage, idDesc);
        }

        auto hdrFBDesc = nvrhi::FramebufferDesc()
            .addColorAttachment(target.Color)
            .setDepthAttachment(target.Depth);
        if (m_ShouldCreateIDTarget)
            hdrFBDesc.addColorAttachment(target.IdBuffer);
        target.HDRFramebuffer = m_NvrhiDevice->createFramebuffer(hdrFBDesc);

        auto ldrFBDesc = nvrhi::FramebufferDesc()
            .addColorAttachment(target.Output);
        target.LDRFramebuffer = m_NvrhiDevice->createFramebuffer(ldrFBDesc);
    }

    void Renderer::PrepareDrawCalls()
    {
        std::vector<GPUInstanceData> allInstanceData;
        m_CurrentFrameData.OpaqueDrawCalls.clear();

        // Flatten all batches into single array
        for (auto& [key, instances] : m_OpaqueBatches)
        {
            if (instances.empty())
                continue;

            uint32_t startOffset = (uint32_t)allInstanceData.size();
            allInstanceData.insert(allInstanceData.end(), instances.begin(), instances.end());

            m_CurrentFrameData.OpaqueDrawCalls.push_back({ key, startOffset, (uint32_t)instances.size() });
        }

        // The batches come out of a map keyed on pointers, in an order that changes from run to run.
        // Where two surfaces lie in the same plane, the one drawn last wins, so the order has to be fixed
        // for a frame to look the same every time.
        std::ranges::sort(m_CurrentFrameData.OpaqueDrawCalls, [](const BatchDrawCall& a, const BatchDrawCall& b)
        {
            const uint64_t meshA = a.Key.Mesh->GetHandle();
            const uint64_t meshB = b.Key.Mesh->GetHandle();
            return std::tie(meshA, a.Key.SubmeshIndex, a.Key.RenderFlags) < std::tie(meshB, b.Key.SubmeshIndex, b.Key.RenderFlags);
        });

        std::ranges::sort(m_CurrentFrameData.TransparentQueue,
          [](const RenderCommand& a, const RenderCommand& b) {
              return a.DistanceToCamera > b.DistanceToCamera; 
          });

        for (auto& cmd : m_CurrentFrameData.TransparentQueue)
        {
            cmd.InstanceOffset = (int)allInstanceData.size();
            allInstanceData.push_back({ cmd.InstanceData.Model, cmd.InstanceData.EntityID });
        }

        m_Frame->MeshInstances.Upload<GPUInstanceData>(*m_Context, *m_Frame->Uploads, allInstanceData, "InstanceBuffer");
        m_CurrentFrameData.InstanceBuffer = m_Frame->MeshInstances.Wrapper;
        m_Profiler.SetCounter("Instances", static_cast<double>(allInstanceData.size()));

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
                if (albedo && albedo->GetTextureHandle())
                {
                    batch.AlbedoTexture = albedo->GetTableIndex();
                    batch.AlbedoSampler = albedo->GetSamplerIndex();
                }
            }

            m_CurrentFrameData.ParticleQueue.push_back(batch);
            allParticleData.insert(allParticleData.end(), particles.begin(), particles.end());
        }

        m_Frame->ParticleInstances.Upload<ParticleInstanceData>(*m_Context, *m_Frame->Uploads, allParticleData, "ParticleInstanceBuffer");
        m_CurrentFrameData.ParticleInstanceAddress = m_Frame->ParticleInstances.Buffer.Address();

        // The copies go straight into the command buffer, so NVRHI must not have a render pass open.
        m_CommandList->clearState();
        m_Frame->Uploads->Flush(NativeCommandBuffer(m_CommandList));
    }

    void Renderer::BeginScene(const glm::mat4& view, const glm::mat4 projection, const glm::vec3& cameraPosition, const glm::vec3& lightDir, const glm::vec3& lightColor, float lightIntensity, float deltaTime, bool editMode)
    {
        ResetStats();
        m_Stats.FrameTime = deltaTime;

        m_OpaqueBatches.clear();
        m_ParticleBatches.clear();
        m_CurrentFrameData.OpaqueDrawCalls.clear();
        m_CurrentFrameData.TransparentQueue.clear();
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
        
        // Flip Y for Vulkan (Y is down in clip space)
        // Handled in Shader Bias Matrix now
        // lightProj[1][1] *= -1.0f; 

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
        if (!m_SceneTarget)
        {
            auto [w, h] = GetViewportSize();
            m_SceneTarget = std::make_unique<RenderTarget>();
            CreateRenderTarget(*m_SceneTarget, w, h);
        }

        m_CurrentFrameData.TargetFramebuffer = m_SceneTarget->HDRFramebuffer;
        m_CurrentFrameData.SceneColorInput = m_SceneTarget->Color;
        if (m_SceneTarget->IdBuffer)
            m_CommandList->clearTextureUInt(m_SceneTarget->IdBuffer, nvrhi::AllSubresources, (uint32_t)-1);
        
        SceneData sceneData;
        sceneData.ViewProjectionMatrix = m_CurrentFrameData.ViewProjection;
        sceneData.LightViewProjection = lightViewProj;
        sceneData.CameraPosition = glm::vec4(cameraPosition, 1.0f);
        sceneData.LightDirection = glm::vec4(lightDir, lightIntensity);
        sceneData.LightColor = glm::vec4(lightColor, 1.0f);
        m_CommandList->writeBuffer(m_GlobalCB, &sceneData, sizeof(SceneData));

        // 3. Clear Screen
        nvrhi::utils::ClearColorAttachment(m_CommandList, m_CurrentFrameData.TargetFramebuffer, 0, nvrhi::Color(0.05f, 0.05f, 0.05f, 1.0f));
        nvrhi::utils::ClearDepthStencilAttachment(m_CommandList, m_CurrentFrameData.TargetFramebuffer, 1.0f, 0);

        m_SubmitStart = std::chrono::steady_clock::now();
    }

    void Renderer::RenderScene(const ExtractedScene& scene)
    {
        BeginScene(scene.View, scene.Projection, scene.CameraPosition, scene.SunDirection, scene.SunColor, scene.SunIntensity, scene.DeltaTime, scene.IsEditorView);

        // A mesh is drawn in the passes whose view it can be seen from: the camera's and the sun's.
        Frustum cameraFrustum;
        cameraFrustum.FromViewProjection(m_CurrentFrameData.ViewProjection);
        Frustum lightFrustum;
        lightFrustum.FromViewProjection(m_CurrentFrameData.LightViewProj);

        for (const ExtractedMesh& mesh : scene.Meshes)
        {
            const AABB worldBounds = TransformAABB(mesh.Mesh->GetBounds(), mesh.Transform);

            // TODO: Check if this is worth it. Using these flags splits the batches up, so more draw calls, but less geometry drawn...
            RenderFlags flags = RenderFlags::None;
            if (cameraFrustum.IsOnFrustum(worldBounds))
                flags = flags | RenderFlags::MainPass;
            if (lightFrustum.IsOnFrustum(worldBounds))
                flags = flags | RenderFlags::ShadowPass;

            if (flags != RenderFlags::None)
                SubmitMesh(mesh.Mesh, mesh.Transform, flags, mesh.EntityId);
        }

        for (const ExtractedParticles& particles : scene.Particles)
            SubmitParticles(particles.Material.get(), particles.Particles);

        m_CurrentFrameData.DebugLines = scene.DebugLines;
        m_CurrentFrameData.Canvases = scene.Canvases;

        EndScene();

        // The lists belong to the caller and may be gone after this call.
        m_CurrentFrameData.DebugLines = {};
        m_CurrentFrameData.Canvases = {};
    }

    void Renderer::SubmitMesh(const std::shared_ptr<StaticMesh>& mesh, const glm::mat4& transform, RenderFlags Flags, int entityID)
    {
        if (!mesh)
            return;

        float dist = glm::distance(m_CurrentFrameData.CameraPosition, glm::vec3(transform[3]));

        for (int i = 0; i < mesh->GetSubmeshes().size(); ++i)
        {
            const auto& submesh = mesh->GetSubmeshes()[i];
            if (!submesh.Material)
                continue;

            GPUInstanceData instance = { transform, entityID };
            BatchKey key = { mesh.get(), (uint32_t)i, submesh.Material.get(), Flags };

            if (submesh.Material->Mode == AlphaMode::Translucent)
            {
                RenderCommand cmd;
                cmd.Mesh = mesh;
                cmd.SubmeshIndex = i;
                cmd.InstanceData = instance;
                cmd.DistanceToCamera = dist;
                cmd.Flags = Flags;
                m_CurrentFrameData.TransparentQueue.push_back(cmd);
            }
            else
            {
                m_OpaqueBatches[key].push_back(instance);
            }
        }
        // 1. Bind Texture
        // Check if mesh has a texture. If so, bind it. If not, use white texture.
        // NOTE: This is expensive to do per-draw-call (creating BindingSet).
        // Optimization for later: Cache BindingSets in the Mesh or a Material.

        // For now, let's reuse SetTexture logic or just call it here.
        // We need to access the AssetManager to get the texture by UUID,
        // OR just assume Mesh holds the Texture pointer directly?
        // You implemented Mesh. Does it hold UUID or shared_ptr?
        // If UUID, we need AssetManager. If ptr, easy.

        // Let's assume for V1 we just use the currently bound texture (from SetTexture)
        // OR we implement a simple lookup.
    }

    TextureSlot Renderer::AddToTextureTable(nvrhi::TextureHandle texture)
    {
        return TextureSlot(*m_Frame->Shared, std::move(texture));
    }

    uint32_t Renderer::SamplerIndex(const SamplerSettings& settings)
    {
        const nvrhi::SamplerHandle sampler = SamplerCache::Get()->GetSampler(settings);
        return m_Frame->Shared->SamplerIndex(vk::Sampler(static_cast<VkSampler>(sampler->getNativeObject(nvrhi::ObjectTypes::VK_Sampler).pointer)));
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
        // Everything between BeginScene and here is deciding what is visible and sorting it into batches.
        m_Profiler.AddCpuTime("Cull", MillisecondsSince(m_SubmitStart));

        {
            ProfileScope prepareScope(m_Profiler, "PrepareDraws");
            PrepareDrawCalls();
        }

        // The editor shows the finished picture inside its viewport panel. A game shows it in the window.
        m_CurrentFrameData.OutputFramebuffer = m_ShouldCreateIDTarget ? m_SceneTarget->LDRFramebuffer.Get() : m_Swapchain->Framebuffer();
        m_CurrentFrameData.SceneColorInput = m_SceneTarget->Color;
        m_CurrentFrameData.SceneColorIndex = *m_Frame->SceneColorIndex;
        m_CurrentFrameData.BloomTextureIndex = m_Frame->BlackTextureIndex;
        m_CurrentFrameData.Bloom = m_BloomSettings;
        m_CurrentFrameData.FXAAEnabled = m_FXAAEnabled;

        m_Graph->Execute(m_RenderContext, m_CurrentFrameData);

        m_Stats.DrawCalls = m_CurrentFrameData.DrawCalls;
        m_Stats.IndexCount = m_CurrentFrameData.IndexCount;
        m_Profiler.SetCounter("DrawCalls", m_Stats.DrawCalls);
        m_Profiler.SetCounter("Triangles", m_Stats.IndexCount / 3);

        m_SceneOutput = m_CurrentFrameData.OutputFramebuffer->getDesc().colorAttachments[0].texture;
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

        m_CommandList->open();
        m_Frame->Timers->BeginFrame(m_Profiler);
        m_Frame->Timers->Begin(NativeCommandBuffer(m_CommandList), "Frame");
        return true;
    }

    void Renderer::EndFrame()
    {
        nvrhi::IFramebuffer* windowFramebuffer = m_Swapchain->Framebuffer();

        if (m_SceneOutput)
        {
            if (!m_ScreenshotRequests.empty())
                CopyFrameForScreenshots(m_SceneOutput);
        }
        else
        {
            // No scene was drawn, for example because it has no camera. Nothing else writes the whole window,
            // and what a swapchain image holds before it is written is undefined.
            nvrhi::utils::ClearColorAttachment(m_CommandList, windowFramebuffer, 0, nvrhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
        }

        m_Frame->Timers->End(NativeCommandBuffer(m_CommandList), "Frame");

        // Submitting and presenting, including the ImGui layer.
        ProfileScope presentScope(m_Profiler, "Present");

        m_CommandList->close();

        // The frame's commands wait for the window's image and signal when it is ready to be shown.
        nvrhi::vulkan::IDevice* vulkanDevice = m_Context->NvrhiVulkan();
        vulkanDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, static_cast<VkSemaphore>(m_Frame->ImageAvailable[m_CurrentFrame]), 0);
        vulkanDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, static_cast<VkSemaphore>(m_Swapchain->RenderFinished()), 0);
        m_NvrhiDevice->executeCommandList(m_CommandList);

        m_ImGuiBackend->render(windowFramebuffer);

        // An empty submission whose only purpose is to signal the fence once everything before it is done.
        m_Context->GraphicsQueue().submit(vk::SubmitInfo(), m_Frame->InFlight[m_CurrentFrame]);

        m_NvrhiDevice->runGarbageCollection();

        m_Swapchain->Present();
        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        ReportMemoryUse();
    }

    void Renderer::RequestScreenshot(uint64_t tag)
    {
        m_ScreenshotRequests.push_back(tag);
    }

    void Renderer::CopyFrameForScreenshots(nvrhi::ITexture* frame)
    {
        const nvrhi::TextureDesc& frameDesc = frame->getDesc();

        for (uint64_t tag : m_ScreenshotRequests)
        {
            nvrhi::TextureDesc stagingDesc;
            stagingDesc.width = frameDesc.width;
            stagingDesc.height = frameDesc.height;
            stagingDesc.format = frameDesc.format;
            stagingDesc.debugName = "Screenshot";

            PendingScreenshot pending;
            pending.Tag = tag;
            pending.Frame = m_Profiler.CurrentFrame();
            pending.Staging = m_NvrhiDevice->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);

            m_CommandList->copyTexture(pending.Staging, nvrhi::TextureSlice(), frame, nvrhi::TextureSlice());
            m_PendingScreenshots.push_back(std::move(pending));
        }

        m_ScreenshotRequests.clear();
    }

    std::vector<Screenshot> Renderer::TakeScreenshots(bool waitForAll)
    {
        if (waitForAll && !m_PendingScreenshots.empty())
            m_NvrhiDevice->waitForIdle();

        std::vector<Screenshot> finished;
        const uint64_t currentFrame = m_Profiler.CurrentFrame();

        for (auto pending = m_PendingScreenshots.begin(); pending != m_PendingScreenshots.end();)
        {
            if (!waitForAll && currentFrame < pending->Frame + ScreenshotReadDelay)
            {
                ++pending;
                continue;
            }

            const nvrhi::TextureDesc& desc = pending->Staging->getDesc();

            size_t rowPitch = 0;
            const auto* source = static_cast<const uint8_t*>(m_NvrhiDevice->mapStagingTexture(pending->Staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch));
            if (source)
            {
                Screenshot screenshot;
                screenshot.Tag = pending->Tag;
                screenshot.Width = desc.width;
                screenshot.Height = desc.height;
                screenshot.Rgba.resize(static_cast<size_t>(desc.width) * desc.height * 4);

                // The frame is BGRA. Its alpha channel does not describe coverage, so the picture is made opaque.
                for (uint32_t y = 0; y < desc.height; ++y)
                {
                    const uint8_t* sourceRow = source + y * rowPitch;
                    uint8_t* targetRow = screenshot.Rgba.data() + static_cast<size_t>(y) * desc.width * 4;
                    for (uint32_t x = 0; x < desc.width; ++x)
                    {
                        targetRow[x * 4 + 0] = sourceRow[x * 4 + 2];
                        targetRow[x * 4 + 1] = sourceRow[x * 4 + 1];
                        targetRow[x * 4 + 2] = sourceRow[x * 4 + 0];
                        targetRow[x * 4 + 3] = 255;
                    }
                }

                m_NvrhiDevice->unmapStagingTexture(pending->Staging);
                finished.push_back(std::move(screenshot));
            }
            else
            {
                LX_CORE_ERROR("Could not read back screenshot {0}.", pending->Tag);
            }

            pending = m_PendingScreenshots.erase(pending);
        }

        return finished;
    }

    void Renderer::FlushGpuTimers()
    {
        m_NvrhiDevice->waitForIdle();
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
    }

    std::pair<nvrhi::BufferHandle, nvrhi::BufferHandle> Renderer::CreateMeshBuffers(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
    {
        auto vbDesc = nvrhi::BufferDesc()
           .setByteSize(vertices.size() * sizeof(Vertex))
           .setIsVertexBuffer(true)
           .setDebugName("MeshVertexBuffer")
           .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::BufferHandle vb = m_NvrhiDevice->createBuffer(vbDesc);

        auto ibDesc = nvrhi::BufferDesc()
            .setByteSize(indices.size() * sizeof(uint32_t))
            .setIsIndexBuffer(true)
            .setDebugName("MeshIndexBuffer")
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        nvrhi::BufferHandle ib = m_NvrhiDevice->createBuffer(ibDesc);

        // TODO: We use a temporary command list here to immediately upload the buffers,
        // in the future, we should maybe batch these things? 
        nvrhi::CommandListHandle commandList = CreateUploadCommandList(m_NvrhiDevice);
        commandList->open();
        commandList->writeBuffer(vb, vertices.data(), vbDesc.byteSize);
        commandList->writeBuffer(ib, indices.data(), ibDesc.byteSize);
        commandList->close();
        m_NvrhiDevice->executeCommandList(commandList);

        return { vb, ib };
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

        if (!m_SceneTarget)
        {
            m_SceneTarget = std::make_unique<RenderTarget>();
            CreateRenderTarget(*m_SceneTarget, width, height);
        }
        else if (m_SceneTarget->Width != width || m_SceneTarget->Height != height)
        {
            // No waiting for the GPU: the old pictures are released once the frames that use them are done.
            m_SceneTarget.reset();
            m_SceneTarget = std::make_unique<RenderTarget>();
            CreateRenderTarget(*m_SceneTarget, width, height);
        }
    }

    nvrhi::TextureHandle Renderer::GetViewportTexture() const
    {
        return m_SceneTarget ? m_SceneTarget->Output : nullptr;
    }

    std::pair<uint32_t, uint32_t> Renderer::GetViewportSize() const
    {
        if (m_SceneTarget)
            return { m_SceneTarget->Width, m_SceneTarget->Height };

        return { m_Swapchain->Width(), m_Swapchain->Height() };
    }
}
