#pragma once

#include <nvrhi/nvrhi.h>

namespace Lynx
{
    class DeletionQueue;

    // The vertices or the indices of a mesh on the GPU. Holds an NVRHI buffer and lets go of it only
    // after the frames that may still draw with it are done. The passes bind the buffer directly,
    // so NVRHI does not know which frames use it and would otherwise destroy it too early when
    // a mesh is unloaded or reloaded.
    class MeshBuffer
    {
    public:
        MeshBuffer() = default;
        MeshBuffer(DeletionQueue& deletions, nvrhi::BufferHandle buffer);
        ~MeshBuffer();

        // A copy refers to the same buffer.
        MeshBuffer(const MeshBuffer& other);
        MeshBuffer& operator=(const MeshBuffer& other);
        MeshBuffer(MeshBuffer&& other) noexcept;
        MeshBuffer& operator=(MeshBuffer&& other) noexcept;

        explicit operator bool() const { return m_Buffer != nullptr; }

        nvrhi::IBuffer* Get() const { return m_Buffer; }

    private:
        void Release();

    private:
        DeletionQueue* m_Deletions = nullptr;
        nvrhi::BufferHandle m_Buffer;
    };
}
