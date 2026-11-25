#include "Rendering.h"

#include <array>

namespace Lynx
{
    void BeginRendering(vk::CommandBuffer commands, const RenderingTargets& targets)
    {
        // The colour target first, then the motion target, then the ID target, each only if the pass has it.
        // A fragment shader's outputs are numbered in that order, and so are a pipeline's formats.
        std::array<vk::RenderingAttachmentInfo, 3> colorAttachments;
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
        for (const vk::ImageView kept : { targets.Motion, targets.Ids })
        {
            if (!kept)
                continue;

            vk::RenderingAttachmentInfo& attachment = colorAttachments[colorCount++];
            attachment.imageView = kept;
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

        const auto width = static_cast<float>(targets.Extent.width);
        const auto height = static_cast<float>(targets.Extent.height);
        commands.setViewport(0, vk::Viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f));
        commands.setScissor(0, vk::Rect2D({ 0, 0 }, targets.Extent));
    }
}
