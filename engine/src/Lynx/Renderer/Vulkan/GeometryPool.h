#pragma once

#include "GpuBuffer.h"
#include "Lynx/Renderer/MeshGeometry.h"

#include <cstddef>
#include <deque>
#include <span>

// The handle types of the memory allocator, declared here so that its large header stays out of this one.
struct VmaVirtualBlock_T;
struct VmaVirtualAllocation_T;

namespace Lynx
{
    class GraphicsContext;
    class UploadQueue;

    // Holds the vertices of all meshes in one buffer and their indices in another. A submesh is a range
    // in each, so a pass binds the two buffers once and draws every mesh from them, and one draw call
    // can cover many different meshes.
    //
    // Ranges are handed out and taken back like memory: a mesh that is reloaded or unloaded leaves
    // a hole that a later mesh fills. The buffers start small and are replaced by larger ones when
    // they fill up. A range keeps its place when that happens.
    class GeometryPool
    {
    public:
        // vertexSize is the size of one vertex in bytes. All meshes in the pool share one vertex layout.
        GeometryPool(GraphicsContext& context, UploadQueue& uploads, uint32_t vertexSize);
        ~GeometryPool();

        GeometryPool(const GeometryPool&) = delete;
        GeometryPool& operator=(const GeometryPool&) = delete;

        // Call at the start of every frame. Ranges that were given back long enough ago become free.
        void BeginFrame();

        // Finds a place for the vertices and indices of a submesh and queues them for upload.
        // They can be drawn from the next frame on. Empty if there is no room left.
        MeshGeometry Add(std::span<const std::byte> vertices, std::span<const uint32_t> indices);

        // Binds the two buffers for drawing. Call it after the pipeline is bound, once per pass.
        void Bind(vk::CommandBuffer commands) const;

        // How much of the buffers is handed out, and how large they are, in bytes.
        uint64_t UsedBytes() const { return m_Vertices.UsedElements * m_Vertices.ElementSize + m_Indices.UsedElements * m_Indices.ElementSize; }
        uint64_t CapacityBytes() const { return m_Vertices.Buffer.Size() + m_Indices.Buffer.Size(); }

    private:
        // One of the two buffers. Its ranges are counted in elements: vertices or indices.
        struct Storage
        {
            GpuBuffer Buffer;
            VmaVirtualBlock_T* Ranges = nullptr;
            uint32_t ElementSize = 0;
            uint64_t SmallestBufferSize = 0;
            uint64_t UsedElements = 0;
            vk::BufferUsageFlagBits Usage{};
            const char* Name = "";
        };

        struct Place;

        // A range that was given back. A frame on the GPU may still draw from it, so it stays taken for a while.
        struct GivenBack
        {
            uint64_t InFrame = 0;
            VmaVirtualAllocation_T* Vertices = nullptr;
            VmaVirtualAllocation_T* Indices = nullptr;
            uint64_t VertexCount = 0;
            uint64_t IndexCount = 0;
        };

        void CreateStorage(Storage& storage, uint32_t elementSize, uint64_t largestElementCount, uint64_t smallestBufferSize, vk::BufferUsageFlagBits usage, const char* name);

        // Takes a range of elements and returns where it starts. False if the storage is full.
        bool Take(Storage& storage, uint64_t elementCount, VmaVirtualAllocation_T*& range, uint64_t& firstElement);
        void GiveBack(const GivenBack& place);
        void Free(const GivenBack& place);

        // Replaces the buffer by a larger one if it ends before endInBytes. What it holds moves along.
        void GrowTo(Storage& storage, uint64_t endInBytes);

    private:
        GraphicsContext& m_Context;
        UploadQueue& m_Uploads;
        Storage m_Vertices;
        Storage m_Indices;
        std::deque<GivenBack> m_GivenBack;
        uint64_t m_Frame = 0;
    };
}
