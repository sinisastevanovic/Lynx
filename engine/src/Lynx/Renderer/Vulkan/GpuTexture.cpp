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

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VkImage image = VK_NULL_HANDLE;
        const VkImageCreateInfo& rawImageInfo = imageInfo;
        const VkResult result = vmaCreateImage(context.Allocator(), &rawImageInfo, &allocationInfo, &image, &m_Allocation, nullptr);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Could not create a GPU texture: " + vk::to_string(static_cast<vk::Result>(result)));

        m_Image = image;
        vmaSetAllocationName(context.Allocator(), m_Allocation, desc.Name);

        vk::ImageViewCreateInfo viewInfo;
        viewInfo.image = m_Image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = desc.Format;
        viewInfo.subresourceRange = vk::ImageSubresourceRange(AspectOf(desc.Format), 0, desc.MipLevels, 0, 1);
        m_View = context.Device().createImageView(viewInfo);
    }

    GpuTexture::~GpuTexture()
    {
        Release();
    }

    GpuTexture::GpuTexture(GpuTexture&& other) noexcept
        : m_Context(std::exchange(other.m_Context, nullptr)),
          m_Image(std::exchange(other.m_Image, nullptr)),
          m_View(std::exchange(other.m_View, nullptr)),
          m_Allocation(std::exchange(other.m_Allocation, nullptr)),
          m_Desc(std::exchange(other.m_Desc, {}))
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
            m_Allocation = std::exchange(other.m_Allocation, nullptr);
            m_Desc = std::exchange(other.m_Desc, {});
        }
        return *this;
    }

    void GpuTexture::Release()
    {
        if (!m_Image)
            return;

        // A frame that is still on the GPU may be drawing into the texture or reading it.
        m_Context->Deletions().Retire([device = m_Context->Device(), allocator = m_Context->Allocator(), image = static_cast<VkImage>(m_Image), view = m_View, allocation = m_Allocation]()
        {
            device.destroyImageView(view);
            vmaDestroyImage(allocator, image, allocation);
        });

        m_Image = nullptr;
        m_View = nullptr;
        m_Allocation = nullptr;
        m_Desc = {};
    }
}
