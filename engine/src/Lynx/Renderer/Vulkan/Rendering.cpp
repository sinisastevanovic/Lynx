#include "Rendering.h"

#include <array>

namespace Lynx
{
    void BeginRendering(vk::CommandBuffer commands, const RenderingTargets& targets)
    {
        // The colour target first, then the ID target, each only if the pass has it.
        std::array<vk::RenderingAttachmentInfo, 2> colorAttachments;
        uint32_t colorCount = 0;
        if (targets.Color)
        {
            vk::RenderingAttachmentInfo& attachment = colorAttachments[colorCount++];
            attachment.imageView = targets.Color;
            attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            attachment.loadOp = targets.ColorLoad;
            attachment.storeOp = vk::AttachmentStoreOp::eStore;
            attachment.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
        }
        if (targets.Ids)
        {
            vk::RenderingAttachmentInfo& attachment = colorAttachments[colorCount++];
            attachment.imageView = targets.Ids;
            attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            attachment.loadOp = vk::AttachmentLoadOp::eLoad;
            attachment.storeOp = vk::AttachmentStoreOp::eStore;
        }

        vk::RenderingAttachmentInfo depthAttachment;
        depthAttachment.imageView = targets.Depth;
        depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        depthAttachment.loadOp = targets.DepthLoad;
        depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        depthAttachment.clearValue = vk::ClearDepthStencilValue(1.0f, 0);

        vk::RenderingInfo renderingInfo;
        renderingInfo.renderArea = vk::Rect2D({ 0, 0 }, targets.Extent);
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = colorCount;
        renderingInfo.pColorAttachments = colorAttachments.data();
        if (targets.Depth)
            renderingInfo.pDepthAttachment = &depthAttachment;
        commands.beginRendering(renderingInfo);

        // NVRHI draws with the viewport turned upside down, so that +Y is up as in Direct3D. The shaders are written
        // for that, so passes on Vulkan get the same viewport until the last NVRHI pass is gone.
        const auto width = static_cast<float>(targets.Extent.width);
        const auto height = static_cast<float>(targets.Extent.height);
        commands.setViewport(0, vk::Viewport(0.0f, height, width, -height, 0.0f, 1.0f));
        commands.setScissor(0, vk::Rect2D({ 0, 0 }, targets.Extent));
    }
}
