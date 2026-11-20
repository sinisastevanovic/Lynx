#pragma once

#include "GpuBuffer.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Lynx
{
    class GraphicsContext;

    // Brings data from the CPU into buffers the GPU reads.
    //
    // Upload copies the bytes into staging memory right away, so the data need not outlive the call.
    // The copies from there into the destinations are recorded together by Flush, into the command buffer
    // of a frame. One upload therefore costs no command buffer and no submission of its own.
    class UploadQueue
    {
    public:
        explicit UploadQueue(GraphicsContext& context);

        // Call at the start of every frame. Staging memory of frames the GPU has finished becomes free again.
        void BeginFrame();

        void Upload(vk::Buffer destination, uint64_t destinationOffset, std::span<const std::byte> data);

        bool HasPending() const { return !m_Copies.empty(); }

        // Records every pending copy. Commands recorded after it may read the destinations.
        // Must not be called inside a render pass.
        void Flush(vk::CommandBuffer commands);

    private:
        // Staging memory is handed out front to back from blocks that are kept and used again,
        // because creating a buffer every frame costs more than the copy it is for.
        struct StagingBlock
        {
            GpuBuffer Buffer;
            uint64_t Used = 0;

            // Holds data that no Flush has recorded yet.
            bool HasPendingData = false;

            // The frame whose Flush last read from the block. The GPU may read it for a few frames after that.
            uint64_t FlushedInFrame = 0;
        };

        struct Copy
        {
            vk::Buffer Staging;
            vk::Buffer Destination;
            vk::BufferCopy Region;
        };

        StagingBlock& BlockWithRoomFor(uint64_t size);
        bool IsFree(const StagingBlock& block) const;

    private:
        GraphicsContext& m_Context;
        std::vector<StagingBlock> m_Blocks;
        std::vector<Copy> m_Copies;
        uint64_t m_Frame = 0;
    };
}
