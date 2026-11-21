#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <vector>

struct VmaAllocation_T;

namespace Lynx
{
    class GraphicsContext;

    struct GpuTextureDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        vk::Format Format = vk::Format::eUndefined;
        vk::ImageUsageFlags Usage;
        uint32_t MipLevels = 1;

        // Also make a view of every mip level on its own, for a texture whose levels are drawn into one by one.
        bool ViewPerMip = false;

        // Shown in RenderDoc and in validation messages.
        const char* Name = "";
    };

    // A two-dimensional picture on the GPU, with a view of all of it. Owns both: when the object goes away,
    // they are released a few frames later, once no frame on the GPU can still be using them.
    //
    // A picture is in one layout at a time, which says what it may be used for: drawn into, read by a shader,
    // copied from. The texture remembers its layout, and Transition changes it. That only covers the picture
    // as a whole: code that puts single mip levels into different layouts has to keep track of them itself.
    class GpuTexture
    {
    public:
        GpuTexture() = default;
        GpuTexture(GraphicsContext& context, const GpuTextureDesc& desc);
        ~GpuTexture();

        GpuTexture(GpuTexture&& other) noexcept;
        GpuTexture& operator=(GpuTexture&& other) noexcept;
        GpuTexture(const GpuTexture&) = delete;
        GpuTexture& operator=(const GpuTexture&) = delete;

        // A texture for a picture that belongs to someone else, such as one of a window's.
        // Only the view is made here and released with the object.
        static GpuTexture Wrap(GraphicsContext& context, vk::Image image, const GpuTextureDesc& desc);

        explicit operator bool() const { return static_cast<bool>(m_Image); }

        vk::Image Image() const { return m_Image; }
        vk::ImageView View() const { return m_View; }
        const GpuTextureDesc& Desc() const { return m_Desc; }

        // Only for a texture made with ViewPerMip.
        vk::ImageView MipView(uint32_t mip) const { return m_MipViews[mip]; }

        vk::ImageLayout Layout() const { return m_Layout; }

        // Brings the picture into a layout. Records nothing if it is in that layout already.
        void Transition(vk::CommandBuffer commands, vk::ImageLayout layout);

        // Fills the whole picture with one colour, or with one depth. Leaves it in the layout for copying into.
        void ClearColor(vk::CommandBuffer commands, const vk::ClearColorValue& color);
        void ClearDepth(vk::CommandBuffer commands, float depth);

    private:
        void CreateViews(GraphicsContext& context);
        void Release();

    private:
        GraphicsContext* m_Context = nullptr;
        vk::Image m_Image;
        vk::ImageView m_View;
        std::vector<vk::ImageView> m_MipViews;

        // Null for a wrapped picture, whose memory is not this object's.
        VmaAllocation_T* m_Allocation = nullptr;
        GpuTextureDesc m_Desc;
        vk::ImageLayout m_Layout = vk::ImageLayout::eUndefined;
    };
}
