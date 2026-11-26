#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <vector>

struct VmaAllocation_T;

namespace Lynx
{
    class GraphicsContext;

    constexpr uint32_t CubeFaceCount = 6;

    struct GpuTextureDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        vk::Format Format = vk::Format::eUndefined;
        vk::ImageUsageFlags Usage;
        uint32_t MipLevels = 1;

        // Also make a view of every mip level on its own, for a texture whose levels are drawn into one by one.
        // For a cube: of every face of every level.
        bool ViewPerMip = false;

        // A cube: six square pictures that a shader reads by a direction instead of by a place, the
        // faces of a cube around the reader. In Vulkan's order: +X, -X, +Y, -Y, +Z, -Z. The width is the
        // length of an edge, and the height has to equal it. Shaders reach it through the table of
        // cubes, see SharedBindings::AddCubeTexture.
        bool IsCube = false;

        // Also make a view that reads the stored bytes as sRGB, so that a shader gets linear light from a
        // picture painted for the screen. Ignored for a format that has no sRGB form.
        bool SrgbView = false;

        // Also make a view that holds only this many of the largest sizes. Read through it, a
        // texture never gives a size smaller than those, however small it is on screen. Zero for none.
        uint32_t FinestSizesView = 0;

        // For a texture with one channel: shaders read its value as red, green and blue alike, with an
        // alpha of 1. That is what a greyscale picture means, whichever channel a shader looks at.
        // Without it, green and blue read as 0.
        bool ReadAsGrey = false;

        // Shown in RenderDoc and in validation messages.
        const char* Name = "";
    };

    // A two-dimensional picture on the GPU, or the six of a cube, with a view of all of it. Owns both: when the object goes away,
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

        // Null unless the texture was made with SrgbView and its format has an sRGB form.
        vk::ImageView SrgbView() const { return m_SrgbView; }

        // Null unless the texture was made with FinestSizesView.
        vk::ImageView FinestSizesView() const { return m_FinestSizesView; }
        const GpuTextureDesc& Desc() const { return m_Desc; }

        // Only for a texture made with ViewPerMip, and not for a cube.
        vk::ImageView MipView(uint32_t mip) const { return m_MipViews[mip]; }

        // One face of one mip level of a cube made with ViewPerMip, as a flat picture to draw into.
        vk::ImageView FaceView(uint32_t face, uint32_t mip) const { return m_MipViews[mip * CubeFaceCount + face]; }

        // How many pictures the texture is made of: six for a cube, one otherwise.
        uint32_t LayerCount() const { return m_Desc.IsCube ? CubeFaceCount : 1; }

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
        vk::ImageView m_SrgbView;
        vk::ImageView m_FinestSizesView;
        std::vector<vk::ImageView> m_MipViews;

        // Null for a wrapped picture, whose memory is not this object's.
        VmaAllocation_T* m_Allocation = nullptr;
        GpuTextureDesc m_Desc;
        vk::ImageLayout m_Layout = vk::ImageLayout::eUndefined;
    };
}
