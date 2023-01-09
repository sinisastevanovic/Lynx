#include "Image.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#include "Application.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Lynx
{
    namespace Utils
    {
        static uint32_t GetVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
        {
            VkPhysicalDeviceMemoryProperties prop;
            vkGetPhysicalDeviceMemoryProperties(Application::GetPhysicalDevice(), &prop);
            for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
            {
                if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
                    return i;
            }

            return 0xffffffff;
        }

        static uint32_t BytesPerPixel(ImageFormat format)
        {
            switch (format)
            {
            case ImageFormat::RGBA:     return 4;
            case ImageFormat::RGBA32F:  return 16;
            default:                    return 0;
            }
        }

        static VkFormat ImageFormatToVulkanFormat(ImageFormat format)
        {
            switch (format)
            {
            case ImageFormat::RGBA:     return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::RGBA32F:  return VK_FORMAT_R32G32B32A32_SFLOAT;
            default:                    return (VkFormat)0;
            }
        }
    }

    Image::Image(std::string_view path)
        : FilePath_(path)
    {
        int width, height, channels;
        uint8_t* data = nullptr;

        if(stbi_is_hdr(FilePath_.c_str()))
        {
            data = (uint8_t*)stbi_loadf(FilePath_.c_str(), &width, &height, &channels, 4);
            Format_ = ImageFormat::RGBA32F;
        }
        else
        {
            data = stbi_load(FilePath_.c_str(), &width, &height, &channels, 4);
            Format_ = ImageFormat::RGBA;
        }

        Width_ = width;
        Height_ = height;

        AllocateMemory(Width_ * Height_ * Utils::BytesPerPixel(Format_));
        SetData(data);
        stbi_image_free(data);
    }

    Image::Image(uint32_t width, uint32_t height, ImageFormat format, const void* data)
        : Width_(width), Height_(height), Format_(format)
    {
        AllocateMemory(Width_ * Height_ * Utils::BytesPerPixel(Format_));
        if(data)
            SetData(data);
    }

    Image::~Image()
    {
        Release();
    }

    void Image::AllocateMemory(uint64_t size)
    {
        VkDevice device = Application::GetDevice();
        VkResult err;
        VkFormat vulkanFormat = Utils::ImageFormatToVulkanFormat(Format_);

        // Create image
        {
            VkImageCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = vulkanFormat;
            info.extent.width = Width_;
            info.extent.height = Height_;
            info.extent.depth = 1;
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            err = vkCreateImage(device, &info, nullptr, &Image_);
            check_vk_result(err);

            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(device, Image_, &req);

            VkMemoryAllocateInfo allocateInfo = {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize = req.size;
            allocateInfo.memoryTypeIndex = Utils::GetVulkanMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
            err = vkAllocateMemory(device, &allocateInfo, nullptr, &Memory_);
            check_vk_result(err);
            err = vkBindImageMemory(device, Image_, Memory_, 0);
            check_vk_result(err);
        }

        // Create image view
        {
            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = Image_;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = vulkanFormat;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
            err = vkCreateImageView(device, &info, nullptr, &ImageView_);
            check_vk_result(err);
        }

        // Create sampler
        {
            VkSamplerCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.minLod = -1000;
            info.maxLod = 1000;
            info.maxAnisotropy = 1.0f;
            err = vkCreateSampler(device, &info, nullptr, &Sampler_);
            check_vk_result(err);
        }

        // Create descriptor set
        DescriptorSet_ = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(Sampler_, ImageView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void Image::Release()
    {
        Application::SubmitResourceFree([sampler = Sampler_, imageView = ImageView_, image = Image_,
            memory = Memory_, stagingBuffer = StagingBuffer_, stagingBufferMemory = StagingBufferMemory_]()
        {
            VkDevice device = Application::GetDevice();

            vkDestroySampler(device, sampler, nullptr);
            vkDestroyImageView(device, imageView, nullptr);
            vkDestroyImage(device, image, nullptr);
            vkFreeMemory(device, memory, nullptr);
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        });

        Sampler_ = nullptr;
        ImageView_ = nullptr;
        Image_ = nullptr;
        Memory_ = nullptr;
        StagingBuffer_ = nullptr;
        StagingBufferMemory_ = nullptr;
    }

    void Image::SetData(const void* data)
    {
        VkDevice device = Application::GetDevice();

        size_t uploadSize = Width_ * Height_ * Utils::BytesPerPixel(Format_);

        VkResult err;

        if(!StagingBuffer_)
        {
            // Create upload buffer
            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = uploadSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(device, &bufferInfo, nullptr, &StagingBuffer_);
            check_vk_result(err);

            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(device, StagingBuffer_, &req);
            AlignedSize_ = req.size;

            VkMemoryAllocateInfo allocateInfo = {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize = req.size;
            allocateInfo.memoryTypeIndex = Utils::GetVulkanMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
            err = vkAllocateMemory(device, &allocateInfo, nullptr, &StagingBufferMemory_);
            check_vk_result(err);
            err = vkBindBufferMemory(device, StagingBuffer_, StagingBufferMemory_, 0);
            check_vk_result(err);
        }

        // Upload to buffer
        {
            char* map = NULL;
            err = vkMapMemory(device, StagingBufferMemory_, 0, AlignedSize_, 0, (void**)(&map));
            check_vk_result(err);
            memcpy(map, data, uploadSize);
            VkMappedMemoryRange range[1] = {};
            range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range[0].memory = StagingBufferMemory_;
            range[0].size = AlignedSize_;
            err = vkFlushMappedMemoryRanges(device, 1, range);
            check_vk_result(err);
            vkUnmapMemory(device, StagingBufferMemory_);
        }

        // Copy to image
        {
            VkCommandBuffer commandBuffer = Application::GetCommandBuffer(true);

            VkImageMemoryBarrier copyBarrier = {};
            copyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            copyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            copyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copyBarrier.image = Image_;
            copyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyBarrier.subresourceRange.levelCount = 1;
            copyBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,  NULL, 1, &copyBarrier);

            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = Width_;
            region.imageExtent.height = Height_;
            region.imageExtent.depth = 1;
            vkCmdCopyBufferToImage(commandBuffer, StagingBuffer_, Image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier useBarrier = {};
            useBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            useBarrier.image = Image_;
            useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            useBarrier.subresourceRange.levelCount = 1;
            useBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &useBarrier);

            Application::FlushCommandBuffer(commandBuffer);
        }
    }

    void Image::Resize(uint32_t width, uint32_t height)
    {
        if(Image_ && Width_ == width && Height_ == height)
            return;

        // TODO: max size?

        Width_ = width;
        Height_ = height;

        Release();
        AllocateMemory(Width_ * Height_ * Utils::BytesPerPixel(Format_));
    }


}
