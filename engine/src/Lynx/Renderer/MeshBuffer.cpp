#include "MeshBuffer.h"

#include "Vulkan/DeletionQueue.h"

#include <utility>

namespace Lynx
{
    MeshBuffer::MeshBuffer(DeletionQueue& deletions, nvrhi::BufferHandle buffer)
        : m_Deletions(&deletions), m_Buffer(std::move(buffer))
    {
    }

    MeshBuffer::~MeshBuffer()
    {
        Release();
    }

    MeshBuffer::MeshBuffer(const MeshBuffer& other) = default;

    MeshBuffer& MeshBuffer::operator=(const MeshBuffer& other)
    {
        if (this != &other)
        {
            Release();
            m_Deletions = other.m_Deletions;
            m_Buffer = other.m_Buffer;
        }
        return *this;
    }

    MeshBuffer::MeshBuffer(MeshBuffer&& other) noexcept
        : m_Deletions(std::exchange(other.m_Deletions, nullptr)), m_Buffer(std::move(other.m_Buffer))
    {
    }

    MeshBuffer& MeshBuffer::operator=(MeshBuffer&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Deletions = std::exchange(other.m_Deletions, nullptr);
            m_Buffer = std::move(other.m_Buffer);
        }
        return *this;
    }

    void MeshBuffer::Release()
    {
        if (!m_Buffer)
            return;

        // The reference moves into the queue, which drops it a few frames from now.
        m_Deletions->Retire([buffer = std::move(m_Buffer)]() {});
        m_Buffer = nullptr;
    }
}
