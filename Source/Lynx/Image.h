#pragma once

#include <string>

#include "vulkan/vulkan.h"

namespace Lynx
{
    enum class ImageFormat
    {
        None = 0,
        RGBA,
        RGBA32F
    };
    
    class Image
    {
    public:
        Image(std::string_view path);
        Image(uint32_t width, uint32_t height, ImageFormat format, const void* data = nullptr);
        ~Image();

        void SetData(const void* data);

        VkDescriptorSet GetDescriptorSet() const { return DescriptorSet_; }

        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const { return Width_; }
        uint32_t GetHeight() const { return Height_; }

    private:
        void AllocateMemory(uint64_t size);
        void Release();

    private:
        uint32_t Width_ = 0, Height_ = 0;

        VkImage Image_ = nullptr;
        VkImageView ImageView_ = nullptr;
        VkDeviceMemory Memory_ = nullptr;
        VkSampler Sampler_ = nullptr;

        ImageFormat Format_ = ImageFormat::None;

        VkBuffer StagingBuffer_ = nullptr;
        VkDeviceMemory StagingBufferMemory_ = nullptr;

        size_t AlignedSize_ = 0;

        VkDescriptorSet DescriptorSet_ = nullptr;

        std::string FilePath_;
    };
}

