#include "GpuBuffer.h"

#include "DeletionQueue.h"
#include "Lynx/Renderer/GraphicsContext.h"

#include <vk_mem_alloc.h>

#include <stdexcept>
#include <utility>

namespace Lynx
{
    GpuBuffer::GpuBuffer(GraphicsContext& context, const GpuBufferDesc& desc)
        : m_Context(&context), m_Size(desc.Size)
    {
        vk::BufferUsageFlags usage = desc.Usage;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (desc.CpuReadable)
        {
            // Memory for writing only is of a kind that is very slow to read from, so reading asks for another kind.
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        else if (desc.CpuWritable)
        {
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        else
        {
            // A buffer the GPU reads can be addressed by a pointer in shaders, which the GPU-driven renderer relies on.
            // Staging buffers must not get this: the allocator would take it as a sign that shaders read them
            // and place them in video memory, where the CPU writes slowly.
            usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
        }

        const VkBufferCreateInfo bufferInfo = vk::BufferCreateInfo({}, desc.Size, usage);

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocationInfo allocated{};
        const VkResult result = vmaCreateBuffer(context.Allocator(), &bufferInfo, &allocationInfo, &buffer, &m_Allocation, &allocated);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Could not create a GPU buffer: " + vk::to_string(static_cast<vk::Result>(result)));

        m_Buffer = buffer;
        m_Data = allocated.pMappedData;
        if (!desc.CpuWritable && !desc.CpuReadable)
            m_Address = context.Device().getBufferAddress(vk::BufferDeviceAddressInfo(m_Buffer));
        vmaSetAllocationName(context.Allocator(), m_Allocation, desc.Name);
    }

    GpuBuffer::~GpuBuffer()
    {
        Release();
    }

    GpuBuffer::GpuBuffer(GpuBuffer&& other) noexcept
        : m_Context(std::exchange(other.m_Context, nullptr)),
          m_Buffer(std::exchange(other.m_Buffer, nullptr)),
          m_Allocation(std::exchange(other.m_Allocation, nullptr)),
          m_Size(std::exchange(other.m_Size, 0)),
          m_Address(std::exchange(other.m_Address, 0)),
          m_Data(std::exchange(other.m_Data, nullptr))
    {
    }

    GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Context = std::exchange(other.m_Context, nullptr);
            m_Buffer = std::exchange(other.m_Buffer, nullptr);
            m_Allocation = std::exchange(other.m_Allocation, nullptr);
            m_Size = std::exchange(other.m_Size, 0);
            m_Address = std::exchange(other.m_Address, 0);
            m_Data = std::exchange(other.m_Data, nullptr);
        }
        return *this;
    }

    void GpuBuffer::Release()
    {
        if (!m_Buffer)
            return;

        // A frame that is still on the GPU may read the buffer, so it is destroyed later.
        m_Context->Deletions().Retire([allocator = m_Context->Allocator(), buffer = static_cast<VkBuffer>(m_Buffer), allocation = m_Allocation]()
        {
            vmaDestroyBuffer(allocator, buffer, allocation);
        });

        m_Buffer = nullptr;
        m_Allocation = nullptr;
        m_Data = nullptr;
        m_Size = 0;
        m_Address = 0;
    }
}
