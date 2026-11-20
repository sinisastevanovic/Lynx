#pragma once

#include <vulkan/vulkan.hpp>

namespace Lynx
{
    // The pictures a run of draws goes into.
    struct RenderingTargets
    {
        // Null for a pass that draws depth only.
        vk::ImageView Color;

        // The picture of entity IDs next to the colour picture. Null for none. Its content is always kept.
        vk::ImageView Ids;

        // Null for none.
        vk::ImageView Depth;

        vk::Extent2D Extent;

        // What the colour picture holds before the first draw: its old content (eLoad), black (eClear),
        // or anything (eDontCare), which is the cheapest and right when every pixel is drawn anyway.
        vk::AttachmentLoadOp ColorLoad = vk::AttachmentLoadOp::eLoad;

        // The same for the depth buffer. Cleared means farthest away.
        vk::AttachmentLoadOp DepthLoad = vk::AttachmentLoadOp::eLoad;
    };

    // Begins rendering into the targets and sets viewport and scissor to cover them. The pictures have to be
    // in the layouts for drawing into them. End with commands.endRendering().
    void BeginRendering(vk::CommandBuffer commands, const RenderingTargets& targets);
}
