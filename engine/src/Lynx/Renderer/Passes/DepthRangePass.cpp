#include "DepthRangePass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/Vulkan/ComputePipeline.h"
#include "Lynx/Renderer/Vulkan/GpuBuffer.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"
#include "Lynx/Renderer/Vulkan/UploadQueue.h"

#include <array>
#include <bit>
#include <span>

namespace Lynx
{
    namespace
    {
        // Has to match the numthreads and the tileSize of DepthRange.slang: a group looks at this many pixels a side.
        constexpr uint32_t PixelsPerGroup = 8 * 8;

        // A range is read this many frames after it was measured, when the GPU has certainly finished that frame.
        constexpr uint64_t ReadbackDelay = 3;

        // Has to match DepthRange in DepthRange.slang: depths as the bits of their floats.
        struct DepthRange
        {
            uint32_t Nearest;
            uint32_t Farthest;
        };

        // Has to match RangeConstants in DepthRange.slang.
        struct RangePushData
        {
            uint64_t Range;
            glm::ivec2 Size;
            uint32_t DepthTexture;

            // The address makes the shader's size a multiple of 8 bytes.
            uint32_t Padding;
        };
    }

    struct DepthRangePass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // What the shader writes the range of the frame into.
        GpuBuffer Range;

        // The range of one frame where the CPU can read it, with what it takes to make distances of it.
        struct Copy
        {
            GpuBuffer Buffer;
            float NearPlane = 0.0f;

            // Zero for a copy that was never made.
            uint64_t Frame = 0;
        };

        // One more than the delay, so that the one being written is never the one being read.
        std::array<Copy, ReadbackDelay + 1> Copies;

        // Counts the frames the pass was asked to take part in, from 1.
        uint64_t Frame = 0;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    DepthRangePass::DepthRangePass() = default;
    DepthRangePass::~DepthRangePass() = default;

    void DepthRangePass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        GpuBufferDesc rangeDesc;
        rangeDesc.Size = sizeof(DepthRange);
        rangeDesc.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc;
        rangeDesc.Name = "DepthRange";
        m_Vulkan->Range = GpuBuffer(*ctx.Graphics, rangeDesc);

        for (VulkanObjects::Copy& copy : m_Vulkan->Copies)
        {
            GpuBufferDesc copyDesc;
            copyDesc.Size = sizeof(DepthRange);
            copyDesc.Usage = vk::BufferUsageFlagBits::eTransferDst;
            copyDesc.CpuReadable = true;
            copyDesc.Name = "DepthRangeForTheCpu";
            copy.Buffer = GpuBuffer(*ctx.Graphics, copyDesc);
        }

        m_PipelineState.SetShader("Shaders/DepthRange.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void DepthRangePass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "DepthRange.slang", sizeof(RangePushData)))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool DepthRangePass::Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // A frame that is not measured still counts, so that no range is taken for a newer one than it is.
        m_Vulkan->Frame++;

        // Depth says how far away something is only in a perspective.
        const bool isPerspective = renderData.Projection[2][3] != 0.0f;
        if (!isPerspective || !renderData.SceneDepth || !m_Vulkan->Pipeline)
            return false;

        // The range starts the wrong way round, so that the first depth found replaces both ends.
        const DepthRange nothingYet{ std::bit_cast<uint32_t>(0.0f), std::bit_cast<uint32_t>(1.0f) };
        ctx.Uploads->Upload(m_Vulkan->Range.Handle(), 0, std::as_bytes(std::span<const DepthRange>(&nothingYet, 1)));

        resources.Sample(renderData.SceneDepth);
        resources.Computes = true;
        return true;
    }

    void DepthRangePass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        VulkanObjects& vulkan = *m_Vulkan;
        const GpuTextureDesc& depth = renderData.SceneDepth->Desc();

        RangePushData push{};
        push.Range = vulkan.Range.Address();
        push.Size = { depth.Width, depth.Height };
        push.DepthTexture = renderData.SceneDepthIndex;

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, vulkan.Pipeline);
        vulkan.Shared->Push(commands, push);
        commands.dispatch(GroupCountFor(depth.Width, PixelsPerGroup), GroupCountFor(depth.Height, PixelsPerGroup), 1);

        // The copy for the CPU has to wait for the shader.
        const vk::MemoryBarrier measured(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead);
        commands.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, measured, nullptr, nullptr);

        VulkanObjects::Copy& copy = vulkan.Copies[vulkan.Frame % vulkan.Copies.size()];
        commands.copyBuffer(vulkan.Range.Handle(), copy.Buffer.Handle(), vk::BufferCopy(0, 0, sizeof(DepthRange)));
        copy.NearPlane = renderData.Projection[3][2] / renderData.Projection[2][2];
        copy.Frame = vulkan.Frame;

        // What an earlier frame measured, if that frame was measured.
        if (vulkan.Frame <= ReadbackDelay)
            return;

        const uint64_t measuredFrame = vulkan.Frame - ReadbackDelay;
        const VulkanObjects::Copy& finished = vulkan.Copies[measuredFrame % vulkan.Copies.size()];
        if (finished.Frame != measuredFrame)
            return;

        const DepthRange& range = *static_cast<const DepthRange*>(finished.Buffer.Data());
        const float nearest = std::bit_cast<float>(range.Nearest);
        const float farthest = std::bit_cast<float>(range.Farthest);

        // Nothing but sky leaves the range as it started. Otherwise a depth is the near plane's distance
        // divided by the surface's, see the reversed depth in Renderer::BeginScene.
        if (nearest > 0.0f && farthest > 0.0f)
            renderData.SeenDepthRange = ViewDepthRange{ finished.NearPlane / nearest, finished.NearPlane / farthest };
    }
}
