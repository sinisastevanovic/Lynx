#include "RenderGraph.h"

#include "Lynx/Profiling/FrameProfiler.h"
#include "Lynx/Renderer/GpuTimers.h"
#include "Lynx/Renderer/Vulkan/Rendering.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"
#include "Lynx/Renderer/Vulkan/UploadQueue.h"

namespace Lynx
{
    namespace
    {
        vk::CommandBuffer NativeCommandBuffer(nvrhi::ICommandList* commandList)
        {
            return vk::CommandBuffer(static_cast<VkCommandBuffer>(commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer).pointer));
        }

        vk::ImageView NativeView(nvrhi::ITexture* texture)
        {
            return vk::ImageView(static_cast<VkImageView>(texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView).pointer));
        }
    }

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
            m_Timers.Begin(NativeCommandBuffer(ctx.CommandList), entry.Name);

            ExecutePass(*entry.Pass, ctx, renderData);

            m_Timers.End(NativeCommandBuffer(ctx.CommandList), entry.Name);
        }
    }

    void RenderGraph::ExecutePass(VulkanPass& pass, RenderContext& ctx, RenderData& renderData)
    {
        PassResources resources;
        if (!pass.Declare(ctx, renderData, resources))
            return;

        // The picture whose size the pass draws at.
        nvrhi::ITexture* mainTarget = resources.ColorTarget ? resources.ColorTarget : resources.DepthTarget;
        if (!mainTarget && !resources.OwnTargets)
            return;

        // Targets that are drawn into together have to be the same size. They can differ for a frame
        // while a window is being resized, and the pass is left out of that frame.
        if (resources.ColorTarget && resources.DepthTarget)
        {
            const nvrhi::TextureDesc& color = resources.ColorTarget->getDesc();
            const nvrhi::TextureDesc& depth = resources.DepthTarget->getDesc();
            if (color.width != depth.width || color.height != depth.height)
                return;
        }

        // NVRHI still records the command buffer, clears the scene's targets and draws the editor's own UI.
        // Handing the command buffer over to a pass takes three steps, which live here so that no pass has to know them.
        // 1. NVRHI tracks which layout every texture is in. It is asked to put the textures into the layouts
        //    the pass needs, and they are left in those layouts, so its bookkeeping stays right.
        for (uint32_t index = 0; index < resources.SampledCount; ++index)
            ctx.CommandList->setTextureState(resources.Sampled[index], nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        if (resources.ColorTarget)
            ctx.CommandList->setTextureState(resources.ColorTarget, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        if (resources.IdTarget)
            ctx.CommandList->setTextureState(resources.IdTarget, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        if (resources.DepthTarget)
            ctx.CommandList->setTextureState(resources.DepthTarget, nvrhi::AllSubresources, nvrhi::ResourceStates::DepthWrite);
        ctx.CommandList->commitBarriers();

        // 2. NVRHI keeps a render pass open between draws and remembers what is bound. Clearing its state
        //    ends that render pass and makes the next NVRHI pass bind everything again.
        ctx.CommandList->clearState();

        // 3. From here on the commands go straight into the command buffer NVRHI is recording.
        const vk::CommandBuffer commands = NativeCommandBuffer(ctx.CommandList);

        // Data the pass queued while declaring, such as this frame's vertices. Copies cannot be recorded
        // once rendering has begun, so this is the last moment for them.
        ctx.Uploads->Flush(commands);

        // Clearing NVRHI's state leaves nothing bound.
        ctx.Shared->Bind(commands);

        if (resources.OwnTargets)
        {
            pass.Execute(commands, renderData);
            return;
        }

        const nvrhi::TextureDesc& targetDesc = mainTarget->getDesc();

        RenderingTargets targets;
        if (resources.ColorTarget)
            targets.Color = NativeView(resources.ColorTarget);
        if (resources.IdTarget)
            targets.Ids = NativeView(resources.IdTarget);
        if (resources.DepthTarget)
            targets.Depth = NativeView(resources.DepthTarget);
        targets.Extent = vk::Extent2D(targetDesc.width, targetDesc.height);
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
