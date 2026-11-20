#include "Rendering.h"

namespace Lynx
{
    void BeginRendering(vk::CommandBuffer commands, const RenderingTargets& targets)
    {
        vk::RenderingAttachmentInfo colorAttachment;
        colorAttachment.imageView = targets.Color;
        colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.loadOp = targets.ColorLoad;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);

        vk::RenderingAttachmentInfo depthAttachment;
        depthAttachment.imageView = targets.Depth;
        depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
        depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;

        vk::RenderingInfo renderingInfo({}, vk::Rect2D({ 0, 0 }, targets.Extent), 1, 0, colorAttachment);
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
