#include "RenderGraph.h"

#include "Lynx/Profiling/FrameProfiler.h"
#include "Lynx/Renderer/GpuTimers.h"
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

    RenderPass& RenderGraph::Add(std::string name, std::unique_ptr<RenderPass> pass, std::function<bool()> condition)
    {
        Entry& entry = m_Passes.emplace_back();
        entry.Name = std::move(name);
        entry.NvrhiPass = std::move(pass);
        entry.Condition = std::move(condition);
        return *entry.NvrhiPass;
    }

    VulkanPass& RenderGraph::Add(std::string name, std::unique_ptr<VulkanPass> pass, std::function<bool()> condition)
    {
        Entry& entry = m_Passes.emplace_back();
        entry.Name = std::move(name);
        entry.DirectPass = std::move(pass);
        entry.Condition = std::move(condition);
        return *entry.DirectPass;
    }

    void RenderGraph::Init(RenderContext& ctx)
    {
        for (Entry& entry : m_Passes)
        {
            if (entry.NvrhiPass)
                entry.NvrhiPass->Init(ctx);
            else
                entry.DirectPass->Init(ctx);
        }
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

            if (entry.NvrhiPass)
                entry.NvrhiPass->Execute(ctx, renderData);
            else
                ExecuteDirectPass(*entry.DirectPass, ctx, renderData);

            m_Timers.End(NativeCommandBuffer(ctx.CommandList), entry.Name);
        }
    }

    void RenderGraph::ExecuteDirectPass(VulkanPass& pass, RenderContext& ctx, RenderData& renderData)
    {
        PassResources resources;
        if (!pass.Declare(ctx, renderData, resources) || !resources.ColorTarget)
            return;

        // Targets that are drawn into together have to be the same size. They can differ for a frame
        // while a window is being resized, and the pass is left out of that frame.
        if (resources.DepthTarget)
        {
            const nvrhi::TextureDesc& color = resources.ColorTarget->getDesc();
            const nvrhi::TextureDesc& depth = resources.DepthTarget->getDesc();
            if (color.width != depth.width || color.height != depth.height)
                return;
        }

        // The hand-over from NVRHI to Vulkan, in three steps. It lives here so that no pass has to know it.
        // 1. NVRHI tracks which layout every texture is in. It is asked to put the textures into the layouts
        //    the pass needs, and they are left in those layouts, so its bookkeeping stays right.
        for (uint32_t index = 0; index < resources.SampledCount; ++index)
            ctx.CommandList->setTextureState(resources.Sampled[index], nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        ctx.CommandList->setTextureState(resources.ColorTarget, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
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

        const nvrhi::TextureDesc& targetDesc= resources.ColorTarget->getDesc();
        const vk::Extent2D extent(targetDesc.width, targetDesc.height);
        const auto width = static_cast<float>(extent.width);
        const auto height = static_cast<float>(extent.height);

        vk::RenderingAttachmentInfo colorAttachment;
        colorAttachment.imageView = NativeView(resources.ColorTarget);
        colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.loadOp = resources.OverwritesTarget ? vk::AttachmentLoadOp::eDontCare : vk::AttachmentLoadOp::eLoad;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

        // A depth buffer is always kept: a pass that declares one draws on top of what others drew.
        vk::RenderingAttachmentInfo depthAttachment;
        if (resources.DepthTarget)
        {
            depthAttachment.imageView = NativeView(resources.DepthTarget);
            depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
            depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        }

        vk::RenderingInfo renderingInfo({}, vk::Rect2D({ 0, 0 }, extent), 1, 0, colorAttachment);
        if (resources.DepthTarget)
            renderingInfo.pDepthAttachment = &depthAttachment;
        commands.beginRendering(renderingInfo);

        // NVRHI draws with the viewport turned upside down, so that +Y is up as in Direct3D. The shaders are written
        // for that, so passes on Vulkan get the same viewport until the last NVRHI pass is gone.
        commands.setViewport(0, vk::Viewport(0.0f, height, width, -height, 0.0f, 1.0f));
        commands.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

        pass.Execute(commands, renderData);

        commands.endRendering();
    }

    void RenderGraph::Clear()
    {
        m_Passes.clear();
    }
}
