#include "DepthPyramidPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/Rendering.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <algorithm>
#include <array>
#include <bit>
#include <tuple>
#include <vector>

namespace Lynx
{
    namespace
    {
        // Has to match PyramidConstants in DepthPyramid.slang.
        struct PyramidPushData
        {
            glm::ivec2 SourceSize;
            glm::ivec2 TargetSize;
            uint32_t SourceTexture;

            // Slang pads push constants to a multiple of their largest member, here a pair of numbers.
            uint32_t Padding;
        };

        // Moves one size of the pyramid between being drawn into and being read by a shader.
        vk::ImageMemoryBarrier2 Transition(vk::Image image, uint32_t mip, vk::ImageLayout from, vk::ImageLayout to)
        {
            const auto usage = [](vk::ImageLayout layout) -> std::pair<vk::PipelineStageFlags2, vk::AccessFlags2>
            {
                switch (layout)
                {
                    case vk::ImageLayout::eColorAttachmentOptimal:
                        return { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite };
                    case vk::ImageLayout::eShaderReadOnlyOptimal:
                        // The next size is drawn from it, and culling reads it in a compute shader.
                        return { vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead };
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

    struct DepthPyramidPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        GpuTexture Pyramid;

        // Where the texture table has each size on its own, to draw the next one from, and all of them
        // together for culling.
        std::vector<TextureSlot> MipSlots;
        TextureSlot PyramidSlot;

        // The layout every size is in. The texture itself only keeps track of one layout for all sizes.
        std::vector<vk::ImageLayout> MipLayouts;

        glm::ivec2 MipSize(uint32_t mip) const
        {
            return { std::max(1u, Pyramid.Desc().Width >> mip), std::max(1u, Pyramid.Desc().Height >> mip) };
        }

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    DepthPyramidPass::DepthPyramidPass() = default;
    DepthPyramidPass::~DepthPyramidPass() = default;

    void DepthPyramidPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/DepthPyramid.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void DepthPyramidPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "DepthPyramid.slang", sizeof(PyramidPushData)))
            return;

        // The vertex shader makes a triangle that covers the target from the vertex index alone.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.ColorFormat = RenderFormats::DepthPyramid;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    void DepthPyramidPass::EnsurePyramid(uint32_t width, uint32_t height)
    {
        VulkanObjects& vulkan = *m_Vulkan;
        if (vulkan.Pyramid && vulkan.Pyramid.Desc().Width == width && vulkan.Pyramid.Desc().Height == height)
            return;

        // Every size down to a single pixel, so that there is one for a thing of any size on screen.
        const uint32_t mipCount = static_cast<uint32_t>(std::bit_width(std::max(width, height)));

        GpuTextureDesc desc;
        desc.Width = width;
        desc.Height = height;
        desc.Format = RenderFormats::DepthPyramid;
        desc.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        desc.MipLevels = mipCount;
        desc.ViewPerMip = true;
        desc.Name = "DepthPyramid";
        vulkan.Pyramid = GpuTexture(*vulkan.Context, desc);

        vulkan.MipSlots.clear();
        for (uint32_t mip = 0; mip < mipCount; ++mip)
            vulkan.MipSlots.emplace_back(*vulkan.Shared, vulkan.Shared->AddTexture(vulkan.Pyramid.MipView(mip)));
        vulkan.PyramidSlot = TextureSlot(*vulkan.Shared, vulkan.Shared->AddTexture(vulkan.Pyramid.View()));
        vulkan.MipLayouts.assign(mipCount, vk::ImageLayout::eUndefined);
    }

    bool DepthPyramidPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // Only culling reads the pyramid.
        if (renderData.Culling.PlaceCount == 0 || !m_Vulkan->Pipeline)
            return false;

        // The largest size is the largest power of two that fits into the depth buffer, so that every
        // size below it is exactly half the one above: a pixel then stands for whole pixels only.
        const GpuTextureDesc& depthDesc = renderData.SceneDepth->Desc();
        EnsurePyramid(std::bit_floor(depthDesc.Width), std::bit_floor(depthDesc.Height));

        // The depth buffer is the only picture of another owner. The pyramid is this pass's own, and so are its layouts.
        resources.Sample(renderData.SceneDepth);
        resources.OwnTargets = true;
        return true;
    }

    void DepthPyramidPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        VulkanObjects& vulkan = *m_Vulkan;
        const vk::Image image = vulkan.Pyramid.Image();
        const uint32_t mipCount = vulkan.Pyramid.Desc().MipLevels;

        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, vulkan.Pipeline);

        PyramidPushData push{};
        push.SourceTexture = renderData.SceneDepthIndex;
        push.SourceSize = { renderData.SceneDepth->Desc().Width, renderData.SceneDepth->Desc().Height };

        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            // Both changes of layout between two sizes go to the GPU as one command.
            std::array<vk::ImageMemoryBarrier2, 2> barriers;
            uint32_t barrierCount = 0;
            if (mip > 0)
            {
                barriers[barrierCount++] = Transition(image, mip - 1, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
                vulkan.MipLayouts[mip - 1] = vk::ImageLayout::eShaderReadOnlyOptimal;
            }
            barriers[barrierCount++] = Transition(image, mip, vulkan.MipLayouts[mip], vk::ImageLayout::eColorAttachmentOptimal);
            vulkan.MipLayouts[mip] = vk::ImageLayout::eColorAttachmentOptimal;

            vk::DependencyInfo dependency;
            dependency.imageMemoryBarrierCount = barrierCount;
            dependency.pImageMemoryBarriers = barriers.data();
            commands.pipelineBarrier2(dependency);

            push.TargetSize = vulkan.MipSize(mip);

            // Every pixel is drawn, so what the size held does not matter.
            RenderingTargets targets;
            targets.Color = vulkan.Pyramid.MipView(mip);
            targets.Extent = vk::Extent2D(push.TargetSize.x, push.TargetSize.y);
            targets.ColorLoad = vk::AttachmentLoadOp::eDontCare;
            BeginRendering(commands, targets);

            vulkan.Shared->Push(commands, push);
            commands.draw(3, 1, 0, 0);
            commands.endRendering();

            push.SourceTexture = vulkan.MipSlots[mip].Index();
            push.SourceSize = push.TargetSize;
        }

        // The smallest size joins the others in the layout for reading.
        const vk::ImageMemoryBarrier2 barrier = Transition(image, mipCount - 1, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        vulkan.MipLayouts[mipCount - 1] = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::DependencyInfo dependency;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        commands.pipelineBarrier2(dependency);

        renderData.DepthPyramid.TextureIndex = vulkan.PyramidSlot.Index();
        renderData.DepthPyramid.Width = vulkan.Pyramid.Desc().Width;
        renderData.DepthPyramid.Height = vulkan.Pyramid.Desc().Height;
        renderData.DepthPyramid.MipCount = mipCount;
    }
}
