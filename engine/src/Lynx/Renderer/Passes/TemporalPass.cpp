#include "TemporalPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/ComputePipeline.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Lynx
{
    namespace
    {
        // Have to match Temporal.slang: the threads of a group a side, the two ways of working, and
        // the views of the pass's working.
        constexpr uint32_t ThreadsPerSide = 8;
        constexpr uint32_t ModeAccumulate = 0;
        constexpr uint32_t ModeResolve = 1;
        constexpr uint32_t ViewNone = 0;

        // A frame counts for at least this much, however long the history is meant to last: below it,
        // the half floats of the history no longer take in what a frame adds.
        constexpr float LeastShareOfAFrame = 0.02f;

        // What stands in for the time since the last frame while the game stands still.
        constexpr float UsualFrameTime = 1.0f / 60.0f;

        // Has to match TemporalConstants in Temporal.slang.
        struct TemporalPushData
        {
            glm::ivec2 Size;
            uint32_t SceneTexture;
            uint32_t HistoryTexture;
            uint32_t OutputTexture;
            uint32_t MotionTexture;
            uint32_t DepthTexture;
            uint32_t PreviousDepthTexture;
            uint32_t DepthOutput;
            uint32_t DebugOutput;
            uint32_t LinearSampler;
            uint32_t Mode;
            uint32_t View;
            uint32_t HasHistory;
            float CurrentWeight;
            float BoxGamma;
            float Sharpness;
            float DepthTolerance;
            float BetweenPixelsShare;
            float StillBoxScale;
            float StillWeight;

            // The shader's size is a multiple of eight bytes, for the pair of numbers it starts with.
            uint32_t Padding;
        };

        // The view of the pass's working that a view mode asks for, as Temporal.slang numbers them.
        uint32_t ViewOf(ViewMode mode)
        {
            switch (mode)
            {
                case ViewMode::TemporalBlend: return 1;
                case ViewMode::TemporalRejected: return 2;
                case ViewMode::TemporalClipped: return 3;
                default: return ViewNone;
            }
        }
    }

    // A picture the pass writes with compute and that shaders read afterwards.
    struct TemporalPicture
    {
        GpuTexture Texture;
        TextureSlot ReadSlot;
        TextureSlot WriteSlot;

        void Create(GraphicsContext& context, SharedBindings& shared, uint32_t width, uint32_t height, vk::Format format, const char* name)
        {
            GpuTextureDesc desc;
            desc.Width = width;
            desc.Height = height;
            desc.Format = format;
            desc.Usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
            desc.Name = name;
            Texture = GpuTexture(context, desc);
            ReadSlot = TextureSlot(shared, shared.AddTexture(Texture.View()));
            WriteSlot = TextureSlot(shared, shared.AddWritableTexture(Texture.View()), TextureTable::Writable);
        }
    };

    struct TemporalPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;
        uint32_t LinearSampler = 0;

        // The result of this frame and of the last, with the number of frames it is made of in its
        // alpha, and the same for the depth of every pixel.
        std::array<TemporalPicture, 2> History;
        std::array<TemporalPicture, 2> Depth;

        // What a view of the pass's working is drawn into.
        TemporalPicture View;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    TemporalPass::TemporalPass() = default;
    TemporalPass::~TemporalPass() = default;

    void TemporalPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        // The history is read between its pixels, and never past the picture's edge.
        SamplerSettings sampler;
        sampler.WrapMode = TextureWrap::Clamp;
        sampler.FilterMode = TextureFilter::Bilinear;
        sampler.UseAnisotropy = false;
        m_Vulkan->LinearSampler = ctx.Shared->SamplerIndex(SamplerCache::Get()->GetSampler(sampler));

        m_PipelineState.SetShader("Shaders/Temporal.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void TemporalPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Temporal.slang", sizeof(TemporalPushData)))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    void TemporalPass::CreatePictures(uint32_t width, uint32_t height)
    {
        GraphicsContext& context = *m_Vulkan->Context;
        SharedBindings& shared = *m_Vulkan->Shared;
        for (uint32_t index = 0; index < 2; ++index)
        {
            m_Vulkan->History[index].Create(context, shared, width, height, RenderFormats::SceneColor, "TemporalHistory");
            m_Vulkan->Depth[index].Create(context, shared, width, height, RenderFormats::DepthPyramid, "TemporalDepth");
        }
        m_Vulkan->View.Create(context, shared, width, height, vk::Format::eR8G8B8A8Unorm, "TemporalView");
    }

    bool TemporalPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        const TemporalSettings& settings = renderData.Temporal;
        if (!m_Vulkan->Pipeline || !renderData.SceneColor || (!settings.Accumulates && !settings.Resolves))
        {
            m_FramesKept = 0;
            return false;
        }

        // Accumulating wins: it is asked for on purpose, to look at or to measure with.
        const bool isAccumulating = settings.Accumulates;
        if (isAccumulating != m_WasAccumulating)
            m_FramesKept = 0;
        m_WasAccumulating = isAccumulating;

        // The pictures are as large as the scene's, and made anew when that changes.
        const GpuTextureDesc& scene = renderData.SceneColor->Desc();
        const GpuTextureDesc& kept = m_Vulkan->History[0].Texture.Desc();
        if (!m_Vulkan->History[0].Texture || kept.Width != scene.Width || kept.Height != scene.Height)
        {
            CreatePictures(scene.Width, scene.Height);
            m_FramesKept = 0;
        }

        // Nothing is taken over across a jump of the camera or a change of scene. And an average of
        // frames that show different things is no picture of anything.
        if (renderData.HistoryWasReset || (isAccumulating && !renderData.ViewIsStill))
            m_FramesKept = 0;

        m_HasHistory = m_FramesKept > 0;
        m_View = isAccumulating ? ViewNone : ViewOf(renderData.Viewing.Mode);
        m_Target = 1 - m_Target;
        const uint32_t last = 1 - m_Target;

        resources.Sample(renderData.SceneColor);
        if (m_HasHistory)
            resources.Sample(&m_Vulkan->History[last].Texture);
        resources.Write(&m_Vulkan->History[m_Target].Texture);
        if (!isAccumulating)
        {
            resources.Sample(renderData.SceneMotion);
            resources.Sample(renderData.SceneDepth);
            if (m_HasHistory)
                resources.Sample(&m_Vulkan->Depth[last].Texture);
            resources.Write(&m_Vulkan->Depth[m_Target].Texture);
            if (m_View != ViewNone)
                resources.Write(&m_Vulkan->View.Texture);
        }
        resources.Computes = true;
        return true;
    }

    void TemporalPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        const TemporalSettings& settings = renderData.Temporal;
        const GpuTextureDesc& scene = renderData.SceneColor->Desc();
        const uint32_t last = 1 - m_Target;

        // Without something of the last frame, the frame's own pictures are named in its place:
        // the shader does not use what it reads there.
        TemporalPushData push{};
        push.Size = { scene.Width, scene.Height };
        push.SceneTexture = renderData.SceneColorIndex;
        push.HistoryTexture = m_HasHistory ? m_Vulkan->History[last].ReadSlot.Index() : renderData.SceneColorIndex;
        push.OutputTexture = m_Vulkan->History[m_Target].WriteSlot.Index();
        push.MotionTexture = renderData.SceneMotionIndex;
        push.DepthTexture = renderData.SceneDepthIndex;
        push.PreviousDepthTexture = m_HasHistory ? m_Vulkan->Depth[last].ReadSlot.Index() : renderData.SceneDepthIndex;
        push.DepthOutput = m_Vulkan->Depth[m_Target].WriteSlot.Index();
        push.DebugOutput = m_Vulkan->View.WriteSlot.Index();
        push.LinearSampler = m_Vulkan->LinearSampler;
        push.Mode = m_WasAccumulating ? ModeAccumulate : ModeResolve;
        push.View = m_View;
        push.HasHistory = m_HasHistory ? 1u : 0u;
        push.BoxGamma = std::max(settings.BoxGamma, 0.0f);
        push.Sharpness = std::clamp(settings.Sharpness, 0.0f, 1.0f);
        push.DepthTolerance = std::max(settings.DepthTolerance, 0.0f);
        push.BetweenPixelsShare = std::clamp(settings.BetweenPixelsShare, 0.0f, 1.0f);
        push.StillBoxScale = std::max(settings.StillBoxScale, 1.0f);

        if (m_WasAccumulating)
        {
            // Every frame counts the same in the average: the new one by its share of all of them so far.
            push.CurrentWeight = 1.0f / static_cast<float>(m_FramesKept + 1);
        }
        else
        {
            // The least a frame counts is set by how long the history is meant to last, not by a number
            // per frame: the picture then looks the same at any frame rate (Jimenez, 2016).
            const float frameTime = renderData.DeltaTime > 0.0f ? renderData.DeltaTime : UsualFrameTime;
            const auto shareFor = [frameTime](float historyTime)
            {
                return std::clamp(1.0f - std::exp(-frameTime / std::max(historyTime, 0.001f)), LeastShareOfAFrame, 1.0f);
            };
            push.CurrentWeight = shareFor(settings.HistoryTime);
            push.StillWeight = shareFor(std::max(settings.StillHistoryTime, settings.HistoryTime));
        }

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);
        m_Vulkan->Shared->Push(commands, push);
        commands.dispatch(GroupCountFor(scene.Width, ThreadsPerSide), GroupCountFor(scene.Height, ThreadsPerSide), 1);
        ++m_FramesKept;

        // The passes after this one work with the result in place of the frame's own picture.
        renderData.SceneColor = &m_Vulkan->History[m_Target].Texture;
        renderData.SceneColorIndex = m_Vulkan->History[m_Target].ReadSlot.Index();
        if (m_View != ViewNone)
        {
            renderData.ShownInstead = &m_Vulkan->View.Texture;
            renderData.ShownInsteadIndex = m_Vulkan->View.ReadSlot.Index();
        }
    }
}
