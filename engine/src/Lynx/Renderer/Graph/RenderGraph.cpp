#include "RenderGraph.h"

#include "Lynx/Profiling/FrameProfiler.h"
#include "Lynx/Renderer/GpuTimers.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/Rendering.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"
#include "Lynx/Renderer/Vulkan/UploadQueue.h"

namespace Lynx
{
    RenderGraph::RenderGraph(FrameProfiler& profiler, GpuTimers& timers)
        : m_Profiler(profiler), m_Timers(timers)
    {
    }

    RenderGraph::~RenderGraph() = default;

    VulkanPass& RenderGraph::Add(std::string name, std::unique_ptr<VulkanPass> pass, std::function<bool()> condition)
    {
        Entry& entry = m_Passes.emplace_back();
        entry.Name = std::move(name);
        entry.Pass = std::move(pass);
        entry.Condition = std::move(condition);
        return *entry.Pass;
    }

    void RenderGraph::Init(RenderContext& ctx)
    {
        for (Entry& entry : m_Passes)
            entry.Pass->Init(ctx);
    }

    void RenderGraph::Execute(RenderContext& ctx, RenderData& renderData)
    {
        for (Entry& entry : m_Passes)
        {
            if (entry.Condition && !entry.Condition())
                continue;

            // The CPU time to record the pass and the GPU time to run it, under the pass's name.
            ProfileScope cpuScope(m_Profiler, entry.Name);
            m_Timers.Begin(vk::CommandBuffer(ctx.Commands), entry.Name);

            ExecutePass(*entry.Pass, ctx, renderData);

            m_Timers.End(vk::CommandBuffer(ctx.Commands), entry.Name);
        }
    }

    void RenderGraph::ExecutePass(VulkanPass& pass, RenderContext& ctx, RenderData& renderData)
    {
        PassResources resources;
        if (!pass.Declare(ctx, renderData, resources))
            return;

        // The picture whose size the pass draws at.
        const GpuTexture* mainTarget = resources.ColorTarget ? resources.ColorTarget : resources.DepthTarget;
        if (!mainTarget && !resources.OwnTargets)
            return;

        // Targets that are drawn into together have to be the same size. They can differ for a frame
        // while a window is being resized, and the pass is left out of that frame.
        if (resources.ColorTarget && resources.DepthTarget)
        {
            const GpuTextureDesc& color = resources.ColorTarget->Desc();
            const GpuTextureDesc& depth = resources.DepthTarget->Desc();
            if (color.Width != depth.Width || color.Height != depth.Height)
                return;
        }

        // Every picture the pass uses is brought into the layout for its use.
        const vk::CommandBuffer commands(ctx.Commands);
        for (uint32_t index = 0; index < resources.SampledCount; ++index)
            resources.Sampled[index]->Transition(commands, vk::ImageLayout::eShaderReadOnlyOptimal);
        if (resources.ColorTarget)
            resources.ColorTarget->Transition(commands, vk::ImageLayout::eColorAttachmentOptimal);
        if (resources.IdTarget)
            resources.IdTarget->Transition(commands, vk::ImageLayout::eColorAttachmentOptimal);
        if (resources.DepthTarget)
            resources.DepthTarget->Transition(commands, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        // Data the pass queued while declaring, such as this frame's vertices. Copies cannot be recorded
        // once rendering has begun, so this is the last moment for them.
        ctx.Uploads->Flush(commands);

        // One set for every pass. It is bound again per pass because a pass may bind other things in between.
        ctx.Shared->Bind(commands);

        if (resources.OwnTargets)
        {
            pass.Execute(commands, renderData);
            return;
        }

        const GpuTextureDesc& targetDesc = mainTarget->Desc();

        RenderingTargets targets;
        if (resources.ColorTarget)
            targets.Color = resources.ColorTarget->View();
        if (resources.IdTarget)
            targets.Ids = resources.IdTarget->View();
        if (resources.DepthTarget)
            targets.Depth = resources.DepthTarget->View();
        targets.Extent = vk::Extent2D(targetDesc.Width, targetDesc.Height);
        targets.ColorLoad = resources.OverwritesTarget ? vk::AttachmentLoadOp::eDontCare : vk::AttachmentLoadOp::eLoad;
        targets.DepthLoad = resources.ClearsDepth ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
        BeginRendering(commands, targets);

        pass.Execute(commands, renderData);

        commands.endRendering();
    }

    void RenderGraph::Clear()
    {
        m_Passes.clear();
    }
}
