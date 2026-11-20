#pragma once

#include <vulkan/vulkan.hpp>

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

        explicit operator bool() const { return static_cast<bool>(m_Image); }

        vk::Image Image() const { return m_Image; }
        vk::ImageView View() const { return m_View; }

        // Only for a texture made with ViewPerMip.
        vk::ImageView MipView(uint32_t mip) const { return m_MipViews[mip]; }
        const GpuTextureDesc& Desc() const { return m_Desc; }

    private:
        void Release();

    private:
        GraphicsContext* m_Context = nullptr;
        vk::Image m_Image;
        vk::ImageView m_View;
        std::vector<vk::ImageView> m_MipViews;
        VmaAllocation_T* m_Allocation = nullptr;
        GpuTextureDesc m_Desc;
    };
}
