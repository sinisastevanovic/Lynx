#pragma once

#include "GpuBuffer.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Lynx
{
    class GraphicsContext;
    class GpuTexture;

    // Brings data from the CPU into buffers and textures the GPU reads.
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

        // Fills the largest size of a texture with pixels and makes every smaller size from the one above it.
        // After the next Flush the texture is in the layout for reading, and stays in it.
        // The texture has to have been created for copying from and into it.
        void UploadTexture(const GpuTexture& texture, std::span<const std::byte> pixels);

        // Copies everything a buffer holds to the start of another one, which takes its place: uploads
        // that are still pending for the old buffer go to the new one. The copy is recorded by the next Flush,
        // ahead of the uploads, and the old buffer is given up after it.
        void MoveContents(GpuBuffer source, vk::Buffer destination);

        bool HasPending() const { return !m_Copies.empty() || !m_TextureCopies.empty() || !m_Moves.empty(); }

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

        struct TextureCopy
        {
            vk::Buffer Staging;
            uint64_t StagingOffset = 0;
            vk::Image Image;
            vk::Extent2D Extent;
            uint32_t MipLevels = 1;
        };

        struct Move
        {
            GpuBuffer Source;
            vk::Buffer Destination;
        };

        StagingBlock& BlockWithRoomFor(uint64_t size);
        void Record(vk::CommandBuffer commands, const TextureCopy& copy) const;
        bool IsFree(const StagingBlock& block) const;

    private:
        GraphicsContext& m_Context;
        std::vector<StagingBlock> m_Blocks;
        std::vector<Copy> m_Copies;
        std::vector<TextureCopy> m_TextureCopies;
        std::vector<Move> m_Moves;
        uint64_t m_Frame = 0;
    };
}
