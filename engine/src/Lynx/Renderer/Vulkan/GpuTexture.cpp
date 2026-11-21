#include "GpuTexture.h"

#include "DeletionQueue.h"
#include "Lynx/Renderer/GraphicsContext.h"

#include <vk_mem_alloc.h>

#include <stdexcept>
#include <utility>

namespace Lynx
{
    namespace
    {
        vk::ImageAspectFlags AspectOf(vk::Format format)
        {
            switch (format)
            {
                case vk::Format::eD16Unorm:
                case vk::Format::eD32Sfloat:
                    return vk::ImageAspectFlagBits::eDepth;
                case vk::Format::eD24UnormS8Uint:
                case vk::Format::eD32SfloatS8Uint:
                    return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                default:
                    return vk::ImageAspectFlagBits::eColor;
            }
        }

        // The format with the same bytes, read as sRGB. Undefined if there is none.
        vk::Format SrgbFormOf(vk::Format format)
        {
            return format == vk::Format::eR8G8B8A8Unorm ? vk::Format::eR8G8B8A8Srgb : vk::Format::eUndefined;
        }

        // What the GPU does with a picture in a layout: at which point of its work, and reading or writing.
        // A change of layout waits for the old use to finish and holds back the new one until the change is done.
        struct LayoutUse
        {
            vk::PipelineStageFlags2 Stages;
            vk::AccessFlags2 Access;
        };

        LayoutUse UseOf(vk::ImageLayout layout)
        {
            switch (layout)
            {
                case vk::ImageLayout::eColorAttachmentOptimal:
                    return { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite };
                case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                    return { vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                             vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite };
                case vk::ImageLayout::eShaderReadOnlyOptimal:
                    return { vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader,
                             vk::AccessFlagBits2::eShaderSampledRead };
                case vk::ImageLayout::eTransferSrcOptimal:
                    return { vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferRead };
                case vk::ImageLayout::eTransferDstOptimal:
                    return { vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferWrite };
                default:
                    // Never used yet, or handed to the window system: nothing on the GPU to wait for.
                    return { vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone };
            }
        }
    }

    GpuTexture::GpuTexture(GraphicsContext& context, const GpuTextureDesc& desc)
        : m_Context(&context), m_Desc(desc)
    {
        vk::ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = desc.Format;
        imageInfo.extent = vk::Extent3D(desc.Width, desc.Height, 1);
        imageInfo.mipLevels = desc.MipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.usage = desc.Usage;

        // A view in another format than the picture's has to be allowed when the picture is made. Naming
        // the formats lets the driver keep the optimisations it would drop for "any format".
        const std::array<vk::Format, 2> viewFormats = { desc.Format, SrgbFormOf(desc.Format) };
        vk::ImageFormatListCreateInfo viewFormatList;
        if (desc.SrgbView && viewFormats[1] != vk::Format::eUndefined)
        {
            viewFormatList.setViewFormats(viewFormats);
            imageInfo.flags = vk::ImageCreateFlagBits::eMutableFormat;
            imageInfo.pNext = &viewFormatList;
        }

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.pool = context.TexturePool();

        VkImage image = VK_NULL_HANDLE;
        const VkImageCreateInfo& rawImageInfo = imageInfo;
        VkResult result = vmaCreateImage(context.Allocator(), &rawImageInfo, &allocationInfo, &image, &m_Allocation, nullptr);
        if (result != VK_SUCCESS)
        {
            // The pool has one kind of memory. A texture that cannot live in it is given whatever fits.
            allocationInfo.pool = nullptr;
            result = vmaCreateImage(context.Allocator(), &rawImageInfo, &allocationInfo, &image, &m_Allocation, nullptr);
        }
        if (result != VK_SUCCESS)
            throw std::runtime_error("Could not create a GPU texture: " + vk::to_string(static_cast<vk::Result>(result)));

        m_Image = image;
        vmaSetAllocationName(context.Allocator(), m_Allocation, desc.Name);
        CreateViews(context);
    }

    GpuTexture GpuTexture::Wrap(GraphicsContext& context, vk::Image image, const GpuTextureDesc& desc)
    {
        GpuTexture texture;
        texture.m_Context = &context;
        texture.m_Image = image;
        texture.m_Desc = desc;
        texture.CreateViews(context);
        return texture;
    }

    void GpuTexture::CreateViews(GraphicsContext& context)
    {
        vk::ImageViewCreateInfo viewInfo;
        viewInfo.image = m_Image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = m_Desc.Format;
        viewInfo.subresourceRange = vk::ImageSubresourceRange(AspectOf(m_Desc.Format), 0, m_Desc.MipLevels, 0, 1);
        m_View = context.Device().createImageView(viewInfo);

        if (m_Desc.SrgbView && SrgbFormOf(m_Desc.Format) != vk::Format::eUndefined)
        {
            viewInfo.format = SrgbFormOf(m_Desc.Format);
            m_SrgbView = context.Device().createImageView(viewInfo);
            viewInfo.format = m_Desc.Format;
        }

        if (m_Desc.ViewPerMip)
        {
            m_MipViews.reserve(m_Desc.MipLevels);
            for (uint32_t mip = 0; mip < m_Desc.MipLevels; ++mip)
            {
                viewInfo.subresourceRange.baseMipLevel = mip;
                viewInfo.subresourceRange.levelCount = 1;
                m_MipViews.push_back(context.Device().createImageView(viewInfo));
            }
        }
    }

    GpuTexture::~GpuTexture()
    {
        Release();
    }

    GpuTexture::GpuTexture(GpuTexture&& other) noexcept
        : m_Context(std::exchange(other.m_Context, nullptr)),
          m_Image(std::exchange(other.m_Image, nullptr)),
          m_View(std::exchange(other.m_View, nullptr)),
          m_SrgbView(std::exchange(other.m_SrgbView, nullptr)),
          m_MipViews(std::move(other.m_MipViews)),
          m_Allocation(std::exchange(other.m_Allocation, nullptr)),
          m_Desc(std::exchange(other.m_Desc, {})),
          m_Layout(std::exchange(other.m_Layout, vk::ImageLayout::eUndefined))
    {
    }

    GpuTexture& GpuTexture::operator=(GpuTexture&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Context = std::exchange(other.m_Context, nullptr);
            m_Image = std::exchange(other.m_Image, nullptr);
            m_View = std::exchange(other.m_View, nullptr);
            m_SrgbView = std::exchange(other.m_SrgbView, nullptr);
            m_MipViews = std::move(other.m_MipViews);
            m_Allocation = std::exchange(other.m_Allocation, nullptr);
            m_Desc = std::exchange(other.m_Desc, {});
            m_Layout = std::exchange(other.m_Layout, vk::ImageLayout::eUndefined);
        }
        return *this;
    }

    void GpuTexture::Transition(vk::CommandBuffer commands, vk::ImageLayout layout)
    {
        if (m_Layout == layout)
            return;

        const LayoutUse before = UseOf(m_Layout);
        const LayoutUse after = UseOf(layout);

        vk::ImageMemoryBarrier2 barrier;
        barrier.srcStageMask = before.Stages;
        barrier.srcAccessMask = before.Access;
        barrier.dstStageMask = after.Stages;
        barrier.dstAccessMask = after.Access;
        barrier.oldLayout = m_Layout;
        barrier.newLayout = layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Image;
        barrier.subresourceRange = vk::ImageSubresourceRange(AspectOf(m_Desc.Format), 0, m_Desc.MipLevels, 0, 1);

        vk::DependencyInfo dependency;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        commands.pipelineBarrier2(dependency);

        m_Layout = layout;
    }

    void GpuTexture::ClearColor(vk::CommandBuffer commands, const vk::ClearColorValue& color)
    {
        Transition(commands, vk::ImageLayout::eTransferDstOptimal);
        commands.clearColorImage(m_Image, vk::ImageLayout::eTransferDstOptimal, color, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, m_Desc.MipLevels, 0, 1));
    }

    void GpuTexture::ClearDepth(vk::CommandBuffer commands, float depth)
    {
        Transition(commands, vk::ImageLayout::eTransferDstOptimal);
        commands.clearDepthStencilImage(m_Image, vk::ImageLayout::eTransferDstOptimal, vk::ClearDepthStencilValue(depth, 0), vk::ImageSubresourceRange(AspectOf(m_Desc.Format), 0, m_Desc.MipLevels, 0, 1));
    }

    void GpuTexture::Release()
    {
        if (!m_Image)
            return;

        // A frame that is still on the GPU may be drawing into the texture or reading it.
        m_Context->Deletions().Retire([device = m_Context->Device(), allocator = m_Context->Allocator(), image = static_cast<VkImage>(m_Image), view = m_View, srgbView = m_SrgbView, mipViews = std::move(m_MipViews), allocation = m_Allocation]()
        {
            device.destroyImageView(view);
            if (srgbView)
                device.destroyImageView(srgbView);
            for (const vk::ImageView mipView : mipViews)
                device.destroyImageView(mipView);

            // A wrapped picture has no memory of its own and is destroyed by its owner.
            if (allocation)
                vmaDestroyImage(allocator, image, allocation);
        });

        m_Image = nullptr;
        m_View = nullptr;
        m_SrgbView = nullptr;
        m_MipViews.clear();
        m_Allocation = nullptr;
        m_Desc = {};
        m_Layout = vk::ImageLayout::eUndefined;
    }
}
