#include "AmbientOcclusionPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/ComputePipeline.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <algorithm>
#include <array>

namespace Lynx
{
    namespace
    {
        // Have to match the shaders: the threads of a group a side, the sizes the distances are kept in,
        // and the pixels a thread of the first step does a side.
        constexpr uint32_t ThreadsPerSide = 8;
        constexpr uint32_t SizeCount = 5;
        constexpr uint32_t PixelsPerDepthThread = 2;

        // The smallest picture all five sizes fit into.
        constexpr uint32_t SmallestPicture = 1u << (SizeCount - 1);

        // XeGTAO works with this many times the radius that is set, and its other numbers were fitted with it.
        constexpr float RadiusMultiplier = 1.457f;

        // How much a pixel counts next to its eight neighbours in the last round of smoothing, in the
        // rounds before it, which smooth more, and what leaves it all but as it is (XeGTAO's numbers).
        constexpr float WeightOfCentre = 1.2f;
        constexpr float WeightOfCentreBeforeLast = WeightOfCentre / 5.0f;
        constexpr float UnsmoothedWeightOfCentre = 1.0e4f;

        // Distances from the camera, where half floats are precise enough and half the memory to read.
        constexpr vk::Format DistanceFormat = vk::Format::eR16Sfloat;

        // The occlusion before it is smoothed, and how much every pixel and its neighbours belong to
        // one surface.
        constexpr vk::Format WorkingFormat = vk::Format::eR8G8Unorm;
        constexpr vk::Format ResultFormat = vk::Format::eR8Unorm;

        // Has to match DepthConstants in AmbientOcclusionDepth.slang.
        struct DepthPushData
        {
            glm::ivec2 Size;
            uint32_t DepthTexture;
            std::array<uint32_t, SizeCount> Sizes;
            float NearPlane;
            float Radius;
        };

        // Has to match OcclusionConstants in AmbientOcclusion.slang.
        struct OcclusionPushData
        {
            glm::ivec2 Size;
            uint32_t DistanceTexture;
            uint32_t OutputTexture;
            glm::vec2 ViewScale;
            glm::vec2 ViewOffset;
            float Radius;
            uint32_t LineCount;
            uint32_t StepCount;
            uint32_t NoiseFrame;
            uint32_t DepthTexture;
            float NearPlane;
        };

        // Has to match DenoiseConstants in AmbientOcclusionDenoise.slang.
        struct DenoisePushData
        {
            glm::ivec2 Size;
            uint32_t OcclusionTexture;
            uint32_t OutputTexture;
            float WeightOfCentre;
            uint32_t IsLast;
        };

        const char* ShaderOf(AmbientOcclusionStep step)
        {
            switch (step)
            {
                case AmbientOcclusionStep::Depth: return "AmbientOcclusionDepth.slang";
                case AmbientOcclusionStep::Occlusion: return "AmbientOcclusion.slang";
                default: return "AmbientOcclusionDenoise.slang";
            }
        }

        size_t PushSizeOf(AmbientOcclusionStep step)
        {
            switch (step)
            {
                case AmbientOcclusionStep::Depth: return sizeof(DepthPushData);
                case AmbientOcclusionStep::Occlusion: return sizeof(OcclusionPushData);
                default: return sizeof(DenoisePushData);
            }
        }

        // How many lines through a pixel, and how many places along each side of a line.
        //
        // Every quality takes three places. XeGTAO's two lower ones take two, which finds less of what
        // stands in the way and comes out lighter: measured on the Motion benchmark against the
        // average of 64 frames at Ultra, two lines of two places are 7.7 off (in steps of 255) and two
        // lines of three places 3.8, for 0.02 ms more. Fewer lines only make a frame grainier, which
        // the frames average away: three lines are 3.4 off.
        struct Sampling
        {
            uint32_t LineCount;
            uint32_t StepCount;
        };

        // How many rounds of the last step a frame takes. Without smoothing it is still one, which
        // leaves every pixel as it is and only brings the result into its final form.
        uint32_t RoundCountOf(const AmbientOcclusionSettings& settings)
        {
            return std::clamp(settings.SmoothingRounds, 1u, AmbientOcclusionPass::MaxSmoothingRounds);
        }

        Sampling SamplingOf(AmbientOcclusionQuality quality)
        {
            switch (quality)
            {
                case AmbientOcclusionQuality::Low: return { 1, 3 };
                case AmbientOcclusionQuality::Medium: return { 2, 3 };
                case AmbientOcclusionQuality::High: return { 3, 3 };
                default: return { 9, 3 };
            }
        }
    }

    struct AmbientOcclusionPictures
    {
        // The distances in their five sizes: one texture, read as a whole and written size by size.
        GpuTexture Distances;
        TextureSlot DistancesRead;
        std::array<TextureSlot, SizeCount> DistancesWrite;

        // The occlusion before it is smoothed. The rounds of smoothing read one and write the other.
        struct WorkingPicture
        {
            GpuTexture Texture;
            TextureSlot Read;
            TextureSlot Write;
        };
        std::array<WorkingPicture, 2> Working;

        GpuTexture Result;
        TextureSlot ResultRead;
        TextureSlot ResultWrite;

        // Set by the first step in a frame that works the occlusion out, and taken back by a step
        // that cannot do its part: the steps after it then leave the frame without.
        bool IsWorkedOut = false;

        void Ensure(GraphicsContext& context, SharedBindings& shared, uint32_t width, uint32_t height)
        {
            if (Result && Result.Desc().Width == width && Result.Desc().Height == height)
                return;

            GpuTextureDesc desc;
            desc.Width = width;
            desc.Height = height;
            desc.Usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;

            desc.Format = WorkingFormat;
            desc.Name = "AmbientOcclusionWorking";
            for (WorkingPicture& working : Working)
            {
                working.Texture = GpuTexture(context, desc);
                working.Read = TextureSlot(shared, shared.AddTexture(working.Texture.View()));
                working.Write = TextureSlot(shared, shared.AddWritableTexture(working.Texture.View()), TextureTable::Writable);
            }

            // Whoever shows the result as a picture sees it grey. A picture is written through a view
            // that leaves its numbers where they are, which the view of a single size is.
            desc.Format = ResultFormat;
            desc.ReadAsGrey = true;
            desc.ViewPerMip = true;
            desc.Name = "AmbientOcclusion";
            Result = GpuTexture(context, desc);
            ResultRead = TextureSlot(shared, shared.AddTexture(Result.View()));
            ResultWrite = TextureSlot(shared, shared.AddWritableTexture(Result.MipView(0)), TextureTable::Writable);

            desc.Format = DistanceFormat;
            desc.ReadAsGrey = false;
            desc.MipLevels = SizeCount;
            desc.Name = "AmbientOcclusionDistances";
            Distances = GpuTexture(context, desc);
            DistancesRead = TextureSlot(shared, shared.AddTexture(Distances.View()));
            for (uint32_t size = 0; size < SizeCount; ++size)
                DistancesWrite[size] = TextureSlot(shared, shared.AddWritableTexture(Distances.MipView(size)), TextureTable::Writable);
        }
    };

    struct AmbientOcclusionPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    std::shared_ptr<AmbientOcclusionPictures> AmbientOcclusionPass::MakePictures()
    {
        return std::make_shared<AmbientOcclusionPictures>();
    }

    AmbientOcclusionPass::AmbientOcclusionPass(AmbientOcclusionStep step, std::shared_ptr<AmbientOcclusionPictures> pictures, uint32_t round)
        : m_Step(step), m_Round(round), m_Pictures(std::move(pictures))
    {
    }

    AmbientOcclusionPass::~AmbientOcclusionPass() = default;

    void AmbientOcclusionPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader(std::filesystem::path("Shaders") / ShaderOf(m_Step));
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void AmbientOcclusionPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, ShaderOf(m_Step), PushSizeOf(m_Step)))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool AmbientOcclusionPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        AmbientOcclusionPictures& pictures = *m_Pictures;
        if (m_Step == AmbientOcclusionStep::Depth)
        {
            pictures.IsWorkedOut = false;
            if (!renderData.AmbientOcclusion.Enabled || !renderData.SceneDepth || ShowsRawColours(renderData.Viewing.Mode))
                return false;

            // The shaders turn a depth into a place in front of the camera as a perspective does.
            const bool isPerspective = renderData.Projection[2][3] != 0.0f;
            const GpuTextureDesc& depth = renderData.SceneDepth->Desc();
            if (!isPerspective || depth.Width < SmallestPicture || depth.Height < SmallestPicture)
                return false;

            pictures.Ensure(*m_Vulkan->Context, *m_Vulkan->Shared, depth.Width, depth.Height);
            pictures.IsWorkedOut = true;
        }

        // A step whose shader has not compiled leaves the frame without occlusion.
        if (!pictures.IsWorkedOut || !m_Vulkan->Pipeline)
        {
            pictures.IsWorkedOut = false;
            return false;
        }

        // There is always one round, which also brings the result into its final form.
        const uint32_t roundCount = RoundCountOf(renderData.AmbientOcclusion);
        if (m_Step == AmbientOcclusionStep::Denoise && m_Round >= roundCount)
            return false;

        switch (m_Step)
        {
            case AmbientOcclusionStep::Depth:
                resources.Sample(renderData.SceneDepth);
                resources.Write(&pictures.Distances);
                break;
            case AmbientOcclusionStep::Occlusion:
                resources.Sample(renderData.SceneDepth);
                resources.Sample(&pictures.Distances);
                resources.Write(&pictures.Working[0].Texture);
                break;
            case AmbientOcclusionStep::Denoise:
                // Round by round from one working picture into the other, and at last into the result.
                resources.Sample(&pictures.Working[m_Round % 2].Texture);
                resources.Write(m_Round + 1 == roundCount ? &pictures.Result : &pictures.Working[(m_Round + 1) % 2].Texture);
                break;
        }
        resources.Computes = true;
        return true;
    }

    void AmbientOcclusionPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        AmbientOcclusionPictures& pictures = *m_Pictures;
        const AmbientOcclusionSettings& settings = renderData.AmbientOcclusion;
        const GpuTextureDesc& picture = pictures.Result.Desc();
        const glm::ivec2 size(picture.Width, picture.Height);
        const float radius = std::max(settings.Radius, 0.01f) * RadiusMultiplier;
        const float nearPlane = renderData.Projection[3][2] / renderData.Projection[2][2];

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);

        if (m_Step == AmbientOcclusionStep::Depth)
        {
            DepthPushData push{};
            push.Size = size;
            push.DepthTexture = renderData.SceneDepthIndex;
            for (uint32_t index = 0; index < SizeCount; ++index)
                push.Sizes[index] = pictures.DistancesWrite[index].Index();
            push.NearPlane = nearPlane;
            push.Radius = radius;
            m_Vulkan->Shared->Push(commands, push);

            const uint32_t pixelsPerGroup = ThreadsPerSide * PixelsPerDepthThread;
            commands.dispatch(GroupCountFor(picture.Width, pixelsPerGroup), GroupCountFor(picture.Height, pixelsPerGroup), 1);
            return;
        }

        if (m_Step == AmbientOcclusionStep::Occlusion)
        {
            // A place on screen and its distance give the place in front of the camera, by how wide
            // the camera sees. The depth buffer was drawn with the frame's jitter, so a pixel shows
            // what lies a part of a pixel next to where it would without.
            const glm::vec2 halfView(1.0f / renderData.Projection[0][0], 1.0f / renderData.Projection[1][1]);
            const Sampling sampling = SamplingOf(settings.Quality);

            OcclusionPushData push{};
            push.Size = size;
            push.DistanceTexture = pictures.DistancesRead.Index();
            push.OutputTexture = pictures.Working[0].Write.Index();
            push.ViewScale = 2.0f * halfView;
            push.ViewOffset = -halfView - push.ViewScale * renderData.Jitter;
            push.Radius = radius;
            push.LineCount = sampling.LineCount;
            push.StepCount = sampling.StepCount;
            push.NoiseFrame = renderData.NoiseFrame;
            push.DepthTexture = renderData.SceneDepthIndex;
            push.NearPlane = nearPlane;
            m_Vulkan->Shared->Push(commands, push);
            commands.dispatch(GroupCountFor(picture.Width, ThreadsPerSide), GroupCountFor(picture.Height, ThreadsPerSide), 1);
            return;
        }

        const bool isLast = m_Round + 1 == RoundCountOf(settings);

        DenoisePushData push{};
        push.Size = size;
        push.OcclusionTexture = pictures.Working[m_Round % 2].Read.Index();
        push.OutputTexture = isLast ? pictures.ResultWrite.Index() : pictures.Working[(m_Round + 1) % 2].Write.Index();
        push.WeightOfCentre = settings.SmoothingRounds == 0 ? UnsmoothedWeightOfCentre : isLast ? WeightOfCentre : WeightOfCentreBeforeLast;
        push.IsLast = isLast ? 1u : 0u;
        m_Vulkan->Shared->Push(commands, push);
        commands.dispatch(GroupCountFor(picture.Width, ThreadsPerSide), GroupCountFor(picture.Height, ThreadsPerSide), 1);
        if (!isLast)
            return;

        // From here on the frame has its occlusion.
        renderData.SceneOcclusion = &pictures.Result;
        renderData.SceneOcclusionIndex = pictures.ResultRead.Index();
        if (renderData.Viewing.Mode == ViewMode::AmbientOcclusion)
        {
            renderData.ShownInstead = &pictures.Result;
            renderData.ShownInsteadIndex = pictures.ResultRead.Index();
        }
    }
}
