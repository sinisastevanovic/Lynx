#include "GeometryPool.h"

#include "DeletionQueue.h"
#include "UploadQueue.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/GraphicsContext.h"

#include <vk_mem_alloc.h>

#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace Lynx
{
    namespace
    {
        // A draw call names its first index with 32 bits and its vertex offset with a signed 32 bits,
        // which is as far as the ranges may reach. The buffers only ever get as large as what is in them.
        constexpr uint64_t LargestVertexCount = uint64_t(1) << 31;
        constexpr uint64_t LargestIndexCount = uint64_t(1) << 32;

        constexpr uint64_t SmallestVertexBufferSize = 16 * 1024 * 1024;
        constexpr uint64_t SmallestIndexBufferSize = 4 * 1024 * 1024;
    }

    // What a MeshGeometry holds on to. The last copy letting go of it gives the place back.
    struct GeometryPool::Place
    {
        GeometryPool& Pool;
        GivenBack Ranges;

        Place(GeometryPool& pool, const GivenBack& ranges) : Pool(pool), Ranges(ranges) {}
        ~Place() { Pool.GiveBack(Ranges); }

        Place(const Place&) = delete;
        Place& operator=(const Place&) = delete;
    };

    GeometryPool::GeometryPool(GraphicsContext& context, UploadQueue& uploads, uint32_t vertexSize)
        : m_Context(context), m_Uploads(uploads)
    {
        CreateStorage(m_Vertices, vertexSize, LargestVertexCount, SmallestVertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer, "GeometryVertices");
        CreateStorage(m_Indices, sizeof(uint32_t), LargestIndexCount, SmallestIndexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer, "GeometryIndices");
    }

    GeometryPool::~GeometryPool()
    {
        // Every mesh is gone by now, so what is left are ranges that were given back in the last few frames.
        for (Storage* storage : { &m_Vertices, &m_Indices })
        {
            vmaClearVirtualBlock(storage->Ranges);
            vmaDestroyVirtualBlock(storage->Ranges);
        }
    }

    void GeometryPool::CreateStorage(Storage& storage, uint32_t elementSize, uint64_t largestElementCount, uint64_t smallestBufferSize, vk::BufferUsageFlagBits usage, const char* name)
    {
        storage.ElementSize = elementSize;
        storage.SmallestBufferSize = smallestBufferSize;
        storage.Usage = usage;
        storage.Name = name;

        // The allocator only keeps the books here: which ranges of elements are taken. It reserves no memory.
        VmaVirtualBlockCreateInfo info{};
        info.size = largestElementCount;
        if (vmaCreateVirtualBlock(&info, &storage.Ranges) != VK_SUCCESS)
            throw std::runtime_error("Could not create the geometry pool.");
    }

    void GeometryPool::BeginFrame()
    {
        ++m_Frame;

        while (!m_GivenBack.empty() && m_GivenBack.front().InFrame + DeletionQueue::FramesToKeep <= m_Frame)
        {
            Free(m_GivenBack.front());
            m_GivenBack.pop_front();
        }
    }

    MeshGeometry GeometryPool::Add(std::span<const std::byte> vertices, std::span<const uint32_t> indices)
    {
        GivenBack ranges;
        ranges.VertexCount = vertices.size() / m_Vertices.ElementSize;
        ranges.IndexCount = indices.size();
        if (ranges.VertexCount == 0 || ranges.IndexCount == 0)
            return {};

        uint64_t firstVertex = 0;
        uint64_t firstIndex = 0;
        if (!Take(m_Vertices, ranges.VertexCount, ranges.Vertices, firstVertex) || !Take(m_Indices, ranges.IndexCount, ranges.Indices, firstIndex))
        {
            LX_CORE_ERROR("The geometry pool has no room for a mesh of {0} vertices and {1} indices.", ranges.VertexCount, ranges.IndexCount);
            Free(ranges);
            return {};
        }

        GrowTo(m_Vertices, (firstVertex + ranges.VertexCount) * m_Vertices.ElementSize);
        GrowTo(m_Indices, (firstIndex + ranges.IndexCount) * m_Indices.ElementSize);

        m_Uploads.Upload(m_Vertices.Buffer.Handle(), firstVertex * m_Vertices.ElementSize, vertices);
        m_Uploads.Upload(m_Indices.Buffer.Handle(), firstIndex * m_Indices.ElementSize, std::as_bytes(indices));

        MeshGeometry geometry;
        geometry.m_FirstIndex = static_cast<uint32_t>(firstIndex);
        geometry.m_VertexOffset = static_cast<int32_t>(firstVertex);
        geometry.m_Place = std::make_shared<Place>(*this, ranges);
        return geometry;
    }

    void GeometryPool::Bind(vk::CommandBuffer commands) const
    {
        // Without a mesh there are no buffers yet, and nothing that could be drawn from them.
        if (!m_Vertices.Buffer || !m_Indices.Buffer)
            return;

        commands.bindVertexBuffers(0, m_Vertices.Buffer.Handle(), vk::DeviceSize(0));
        commands.bindIndexBuffer(m_Indices.Buffer.Handle(), 0, vk::IndexType::eUint32);
    }

    bool GeometryPool::Take(Storage& storage, uint64_t elementCount, VmaVirtualAllocation_T*& range, uint64_t& firstElement)
    {
        // The lowest free range that fits is taken, which keeps the buffer as small as its content allows.
        VmaVirtualAllocationCreateInfo info{};
        info.size = elementCount;
        info.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT;
        if (vmaVirtualAllocate(storage.Ranges, &info, &range, &firstElement) != VK_SUCCESS)
        {
            range = nullptr;
            return false;
        }

        storage.UsedElements += elementCount;
        return true;
    }

    void GeometryPool::GiveBack(const GivenBack& place)
    {
        GivenBack givenBack = place;
        givenBack.InFrame = m_Frame;
        m_GivenBack.push_back(givenBack);
    }

    void GeometryPool::Free(const GivenBack& place)
    {
        if (place.Vertices)
        {
            vmaVirtualFree(m_Vertices.Ranges, place.Vertices);
            m_Vertices.UsedElements -= place.VertexCount;
        }
        if (place.Indices)
        {
            vmaVirtualFree(m_Indices.Ranges, place.Indices);
            m_Indices.UsedElements -= place.IndexCount;
        }
    }

    void GeometryPool::GrowTo(Storage& storage, uint64_t endInBytes)
    {
        if (endInBytes <= storage.Buffer.Size())
            return;

        // Doubling keeps the number of times a pool is replaced small, however many meshes arrive one by one.
        GpuBufferDesc desc;
        desc.Size = std::max(storage.SmallestBufferSize, std::bit_ceil(endInBytes));
        desc.Usage = storage.Usage | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc;
        desc.Name = storage.Name;
        GpuBuffer larger(m_Context, desc);

        // Frames that are still on the GPU keep drawing from the old buffer, which lives on until they are done.
        if (storage.Buffer)
            m_Uploads.MoveContents(std::move(storage.Buffer), larger.Handle());
        storage.Buffer = std::move(larger);
    }
}
