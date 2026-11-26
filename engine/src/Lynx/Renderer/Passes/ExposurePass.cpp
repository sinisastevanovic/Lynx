#include "ExposurePass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/Vulkan/ComputePipeline.h"
#include "Lynx/Renderer/Vulkan/GpuBuffer.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"
#include "Lynx/Renderer/Vulkan/UploadQueue.h"

#include <array>
#include <cmath>
#include <span>

namespace Lynx
{
    namespace
    {
        // Have to match Exposure.slang: the bins of the histogram, its steps, and the threads of a group a side.
        constexpr size_t BinCount = 256;
        constexpr uint32_t StepCount = 0;
        constexpr uint32_t StepAdapt = 1;
        constexpr uint32_t ThreadsPerSide = 8;

        // One pixel in this many across and down is looked at. A histogram needs a fair sample of the
        // picture, not all of it: at 1920 by 1080 this still counts 32,000 places. With one in four,
        // 130,000 places, the pass took 0.036 ms instead of 0.022: many of them add to the same bin
        // at the same time and have to wait for each other.
        constexpr uint32_t PixelsPerPlace = 8;

        // Has to match ExposureConstants in Exposure.slang.
        struct ExposurePushData
        {
            uint64_t Histogram;
            glm::ivec2 Places;
            glm::ivec2 PictureSize;
            uint32_t SceneTexture;
            uint32_t Step;
            float DeltaTime;
            float FixedExposure;
            float Compensation;
            float Lowest;
            float Highest;
            float SpeedTowardsDarker;
            float SpeedTowardsBrighter;
            uint32_t Padding;
        };
    }

    struct ExposurePass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // How many places of the picture have which brightness. Emptied before every count.
        GpuBuffer Histogram;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    ExposurePass::ExposurePass() = default;
    ExposurePass::~ExposurePass() = default;

    void ExposurePass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        GpuBufferDesc histogramDesc;
        histogramDesc.Size = BinCount * sizeof(uint32_t);
        histogramDesc.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        histogramDesc.Name = "ExposureHistogram";
        m_Vulkan->Histogram = GpuBuffer(*ctx.Graphics, histogramDesc);

        m_PipelineState.SetShader("Shaders/Exposure.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void ExposurePass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Exposure.slang", sizeof(ExposurePushData)))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool ExposurePass::Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // Raw colours are shown as they are, whatever their brightness.
        if (!m_Vulkan->Pipeline || !renderData.SceneColor || ShowsRawColours(renderData.Viewing.Mode))
            return false;

        if (renderData.Exposure.IsAutomatic)
        {
            const std::array<uint32_t, BinCount> empty{};
            ctx.Uploads->Upload(m_Vulkan->Histogram.Handle(), 0, std::as_bytes(std::span(empty)));
        }

        resources.Sample(renderData.SceneColor);
        resources.Computes = true;
        return true;
    }

    void ExposurePass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        const ExposureSettings& settings = renderData.Exposure;
        const GpuTextureDesc& picture = renderData.SceneColor->Desc();

        ExposurePushData push{};
        push.Histogram = m_Vulkan->Histogram.Address();
        push.PictureSize = { picture.Width, picture.Height };
        push.Places = { std::max(1u, picture.Width / PixelsPerPlace), std::max(1u, picture.Height / PixelsPerPlace) };
        push.SceneTexture = renderData.SceneColorIndex;
        push.DeltaTime = renderData.DeltaTime;
        push.FixedExposure = settings.IsAutomatic ? -1.0f : std::max(settings.Fixed, 0.0f);
        push.Compensation = std::exp2(settings.Compensation);
        push.Lowest = std::max(settings.Lowest, 0.0001f);
        push.Highest = std::max(settings.Highest, push.Lowest);
        push.SpeedTowardsDarker = std::max(settings.SpeedTowardsDarker, 0.0f);
        push.SpeedTowardsBrighter = std::max(settings.SpeedTowardsBrighter, 0.0f);

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);

        if (settings.IsAutomatic)
        {
            push.Step = StepCount;
            m_Vulkan->Shared->Push(commands, push);
            commands.dispatch(GroupCountFor(push.Places.x, ThreadsPerSide), GroupCountFor(push.Places.y, ThreadsPerSide), 1);

            // The second step reads what the first one counted.
            const vk::MemoryBarrier counted(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
            commands.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, counted, nullptr, nullptr);
        }

        push.Step = StepAdapt;
        m_Vulkan->Shared->Push(commands, push);
        commands.dispatch(1, 1, 1);
    }
}
