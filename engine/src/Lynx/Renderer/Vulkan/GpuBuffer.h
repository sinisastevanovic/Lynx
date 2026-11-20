#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>

// The handle type of the memory allocator, declared here so that its large header stays out of this one.
struct VmaAllocation_T;

namespace Lynx
{
    class GraphicsContext;

    struct GpuBufferDesc
    {
        uint64_t Size = 0;
        vk::BufferUsageFlags Usage;

        // Memory the CPU can write to directly, at the price of slower access from the GPU.
        // Used for staging: data is written there and then copied into a buffer the GPU reads fast.
        bool CpuWritable = false;

        // Shown in RenderDoc and in validation messages.
        const char* Name = "";
    };

    // A block of GPU memory. Owns it: when the object goes away, the memory is released a few frames later,
    // once no frame on the GPU can still be reading it.
    class GpuBuffer
    {
    public:
        GpuBuffer() = default;
        GpuBuffer(GraphicsContext& context, const GpuBufferDesc& desc);
        ~GpuBuffer();

        GpuBuffer(GpuBuffer&& other) noexcept;
        GpuBuffer& operator=(GpuBuffer&& other) noexcept;
        GpuBuffer(const GpuBuffer&) = delete;
        GpuBuffer& operator=(const GpuBuffer&) = delete;

        explicit operator bool() const { return static_cast<bool>(m_Buffer); }

        vk::Buffer Handle() const { return m_Buffer; }
        uint64_t Size() const { return m_Size; }

        // The address shaders reach the buffer by, without it being bound. Zero for a CpuWritable buffer.
        vk::DeviceAddress Address() const { return m_Address; }

        // Where the CPU can write. Null unless the buffer was created CpuWritable.
        void* Data() const { return m_Data; }

    private:
        void Release();

    private:
        GraphicsContext* m_Context = nullptr;
        vk::Buffer m_Buffer;
        VmaAllocation_T* m_Allocation = nullptr;
        uint64_t m_Size = 0;
        vk::DeviceAddress m_Address = 0;
        void* m_Data = nullptr;
    };
}
