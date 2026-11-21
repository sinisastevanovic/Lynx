#pragma once

#include <memory>

namespace Lynx
{
    class GpuBuffer;

    // The vertices or the indices of a mesh on the GPU. A copy refers to the same buffer, which is released
    // when the last copy goes away, and then only after the frames that may still draw with it are done.
    class MeshBuffer
    {
    public:
        MeshBuffer() = default;
        explicit MeshBuffer(std::shared_ptr<GpuBuffer> buffer) : m_Buffer(std::move(buffer)) {}

        explicit operator bool() const { return m_Buffer != nullptr; }

        const GpuBuffer& Get() const { return *m_Buffer; }

    private:
        std::shared_ptr<GpuBuffer> m_Buffer;
    };
}
