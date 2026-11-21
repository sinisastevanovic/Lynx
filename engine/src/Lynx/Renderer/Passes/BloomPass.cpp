#include "BloomPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/Rendering.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <tuple>

namespace Lynx
{
    namespace
    {
        constexpr uint32_t MaxMips = 6;

        // Has to match the mode constants in Bloom.slang.
        enum class BloomMode : int32_t
        {
            Prefilter = 0,
            Downsample = 1,
            Upsample = 2
        };

        // Has to match BloomConstants in Bloom.slang.
        struct BloomPushData
        {
            float Threshold;
            float Knee;
            float Radius;
            BloomMode Mode;
            glm::vec2 TexelSize;
            uint32_t SourceTexture;
            uint32_t LinearSampler;
        };

        // Moves one size of the picture between being drawn into and being read by a shader.
        vk::ImageMemoryBarrier2 Transition(vk::Image image, uint32_t mip, vk::ImageLayout from, vk::ImageLayout to)
        {
            const auto usage = [](vk::ImageLayout layout) -> std::pair<vk::PipelineStageFlags2, vk::AccessFlags2>
            {
                switch (layout)
                {
                    case vk::ImageLayout::eColorAttachmentOptimal:
                        return { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead };
                    case vk::ImageLayout::eShaderReadOnlyOptimal:
                        return { vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead };
                    default:
                        // Never used yet, so there is nothing to wait for.
                        return { vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone };
                }
            };

            vk::ImageMemoryBarrier2 barrier;
            std::tie(barrier.srcStageMask, barrier.srcAccessMask) = usage(from);
            std::tie(barrier.dstStageMask, barrier.dstAccessMask) = usage(to);
            barrier.oldLayout = from;
            barrier.newLayout = to;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mip, 1, 0, 1);
            return barrier;
        }
    }

    struct BloomPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        // The same shader twice: one replaces what the target holds, the other adds to it.
        vk::Pipeline ReplacePipeline;
        vk::Pipeline AddPipeline;

        // The bloom picture in all its sizes, each half the one before. Size 0 holds the result.
        GpuTexture Chain;
        uint32_t MipCount = 0;

        // Where the texture table has each size on its own, and all of them together for the composite pass.
        std::array<TextureSlot, MaxMips> MipSlots;
        TextureSlot ChainSlot;

        // The layout every size is in. The texture itself only keeps track of one layout for all sizes.
        std::array<vk::ImageLayout, MaxMips> MipLayouts{};

        vk::Extent2D MipExtent(uint32_t mip) const
        {
            return { std::max(1u, Chain.Desc().Width >> mip), std::max(1u, Chain.Desc().Height >> mip) };
        }

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(ReplacePipeline);
            Device.destroyPipeline(AddPipeline);
        }
    };

    BloomPass::BloomPass() = default;
    BloomPass::~BloomPass() = default;

    void BloomPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        // Clamped, so that the samples around a pixel at the edge do not reach the opposite edge.
        m_LinearSampler = ctx.Shared->SamplerIndex(SamplerCache::Get()->GetSampler(SamplerSettings{ TextureWrap::Clamp, TextureFilter::Bilinear }));

        m_PipelineState.SetShader("Shaders/Bloom.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipelines(*shader); });
    }

    void BloomPass::CreatePipelines(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipelines that work are kept.
        if (!m_Vulkan->Shared->Supports(*program, "Bloom.slang", sizeof(BloomPushData)))
            return;

        // The vertex shader makes a triangle that covers the target from the vertex index alone.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.ColorFormat = RenderFormats::SceneColor;
        const vk::Pipeline replacePipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        desc.Blend = BlendMode::Additive;
        const vk::Pipeline addPipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        // A reloaded shader replaces pipelines that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, oldReplace = m_Vulkan->ReplacePipeline, oldAdd = m_Vulkan->AddPipeline]()
        {
            device.destroyPipeline(oldReplace);
            device.destroyPipeline(oldAdd);
        });
        m_Vulkan->ReplacePipeline = replacePipeline;
        m_Vulkan->AddPipeline = addPipeline;
    }

    void BloomPass::EnsureChain(uint32_t width, uint32_t height)
    {
        if (m_Vulkan->Chain && m_Vulkan->Chain.Desc().Width == width && m_Vulkan->Chain.Desc().Height == height)
            return;

        // As many sizes as there are until the picture is one pixel, but no more than MaxMips.
        m_Vulkan->MipCount = std::min(MaxMips, static_cast<uint32_t>(std::bit_width(std::max(width, height))));

        GpuTextureDesc desc;
        desc.Width = width;
        desc.Height = height;
        desc.Format = RenderFormats::SceneColor;
        desc.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        desc.MipLevels = m_Vulkan->MipCount;
        desc.ViewPerMip = true;
        desc.Name = "BloomChain";
        m_Vulkan->Chain = GpuTexture(*m_Vulkan->Context, desc);

        for (uint32_t mip = 0; mip < m_Vulkan->MipCount; ++mip)
            m_Vulkan->MipSlots[mip] = TextureSlot(*m_Vulkan->Shared, m_Vulkan->Shared->AddTexture(m_Vulkan->Chain.MipView(mip)));
        m_Vulkan->ChainSlot = TextureSlot(*m_Vulkan->Shared, m_Vulkan->Shared->AddTexture(m_Vulkan->Chain.View()));
        m_Vulkan->MipLayouts.fill(vk::ImageLayout::eUndefined);
    }

    bool BloomPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipelines(*shader); });

        if (!renderData.Bloom.Enabled || !m_Vulkan->ReplacePipeline || !m_Vulkan->AddPipeline)
            return false;

        const GpuTextureDesc& sceneDesc = renderData.SceneColor->Desc();
        EnsureChain(std::max(1u, sceneDesc.Width / 2), std::max(1u, sceneDesc.Height / 2));

        // The scene is the only picture of another owner. The chain is this pass's own, and so are its layouts.
        resources.Sample(renderData.SceneColor);
        resources.OwnTargets = true;
        return true;
    }

    void BloomPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        VulkanObjects& vulkan = *m_Vulkan;
        const vk::Image image = vulkan.Chain.Image();

        BloomPushData push{};
        push.Threshold = renderData.Bloom.Threshold;
        push.Knee = renderData.Bloom.Knee;
        push.Radius = renderData.Bloom.Radius;
        push.LinearSampler = m_LinearSampler;

        // The size drawn into by the step before, which the next step reads.
        std::optional<uint32_t> previousTarget;

        // One step: a triangle over one size of the chain, reading another picture.
        const auto step = [&](uint32_t target, uint32_t sourceTexture, vk::Extent2D sourceExtent, BloomMode mode, vk::Pipeline pipeline, vk::AttachmentLoadOp load)
        {
            // Both changes of layout between two steps go to the GPU as one command.
            std::array<vk::ImageMemoryBarrier2, 2> barriers;
            uint32_t barrierCount = 0;
            if (previousTarget)
            {
                barriers[barrierCount++] = Transition(image, *previousTarget, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
                vulkan.MipLayouts[*previousTarget] = vk::ImageLayout::eShaderReadOnlyOptimal;
            }
            barriers[barrierCount++] = Transition(image, target, vulkan.MipLayouts[target], vk::ImageLayout::eColorAttachmentOptimal);
            vulkan.MipLayouts[target] = vk::ImageLayout::eColorAttachmentOptimal;

            vk::DependencyInfo dependency;
            dependency.imageMemoryBarrierCount = barrierCount;
            dependency.pImageMemoryBarriers = barriers.data();
            commands.pipelineBarrier2(dependency);

            RenderingTargets targets;
            targets.Color = vulkan.Chain.MipView(target);
            targets.Extent = vulkan.MipExtent(target);
            targets.ColorLoad = load;
            BeginRendering(commands, targets);

            push.Mode = mode;
            push.SourceTexture = sourceTexture;
            push.TexelSize = 1.0f / glm::vec2(sourceExtent.width, sourceExtent.height);

            commands.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
            vulkan.Shared->Push(commands, push);
            commands.draw(3, 1, 0, 0);
            commands.endRendering();

            previousTarget = target;
        };

        // The bright parts of the scene go into the largest size. Every pixel is drawn, so what it held does not matter.
        const GpuTextureDesc& sceneDesc = renderData.SceneColor->Desc();
        step(0, renderData.SceneColorIndex, { sceneDesc.Width, sceneDesc.Height }, BloomMode::Prefilter, vulkan.ReplacePipeline, vk::AttachmentLoadOp::eDontCare);

        // Down: every size is made from the one above it.
        for (uint32_t mip = 1; mip < vulkan.MipCount; ++mip)
            step(mip, vulkan.MipSlots[mip - 1].Index(), vulkan.MipExtent(mip - 1), BloomMode::Downsample, vulkan.ReplacePipeline, vk::AttachmentLoadOp::eDontCare);

        // Up: every size is added to the one above it, so the largest ends up with the glow of all of them.
        for (uint32_t mip = vulkan.MipCount - 1; mip > 0; --mip)
            step(mip - 1, vulkan.MipSlots[mip].Index(), vulkan.MipExtent(mip), BloomMode::Upsample, vulkan.AddPipeline, vk::AttachmentLoadOp::eLoad);

        // The result is left ready for the composite pass to read.
        if (previousTarget)
        {
            const vk::ImageMemoryBarrier2 barrier = Transition(image, *previousTarget, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            vulkan.MipLayouts[*previousTarget] = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::DependencyInfo dependency;
            dependency.imageMemoryBarrierCount = 1;
            dependency.pImageMemoryBarriers = &barrier;
            commands.pipelineBarrier2(dependency);
        }

        renderData.BloomTextureIndex = vulkan.ChainSlot.Index();
    }
}
