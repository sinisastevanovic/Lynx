#include "UploadQueue.h"

#include "DeletionQueue.h"
#include "GpuTexture.h"
#include "Lynx/Renderer/GraphicsContext.h"

#include <algorithm>
#include <bit>
#include <cstring>

namespace Lynx
{
    namespace
    {
        constexpr uint64_t SmallestBlockSize = 256 * 1024;

        // A free block that nothing has used for this many frames is given back.
        constexpr uint64_t FramesToKeepUnusedBlocks = 600;
    }

    UploadQueue::UploadQueue(GraphicsContext& context)
        : m_Context(context)
    {
    }

    void UploadQueue::BeginFrame()
    {
        ++m_Frame;

        std::erase_if(m_Blocks, [this](const StagingBlock& block)
        {
            return IsFree(block) && block.FlushedInFrame + FramesToKeepUnusedBlocks <= m_Frame;
        });
    }

    bool UploadQueue::IsFree(const StagingBlock& block) const
    {
        return !block.HasPendingData && block.FlushedInFrame + DeletionQueue::FramesToKeep <= m_Frame;
    }

    namespace
    {
        // Where the next piece of data may start in a staging block, after one that ends at the given
        // place: on a multiple of 16 bytes. Pixels have to start on a multiple of the size of a texel,
        // and no format the engine uses has texels larger than that. Without it, a texture of 16-bit
        // texels with an odd number of them left the next texture of four-byte texels two bytes off.
        constexpr uint64_t NextPlace(uint64_t end)
        {
            constexpr uint64_t alignment = 16;
            return (end + alignment - 1) / alignment * alignment;
        }
    }

    UploadQueue::StagingBlock& UploadQueue::BlockWithRoomFor(uint64_t size)
    {
        for (StagingBlock& block : m_Blocks)
        {
            if (block.HasPendingData && block.Used + size <= block.Buffer.Size())
                return block;
        }

        for (StagingBlock& block : m_Blocks)
        {
            if (IsFree(block) && size <= block.Buffer.Size())
            {
                block.Used = 0;
                return block;
            }
        }

        GpuBufferDesc desc;
        desc.Size = std::max(SmallestBlockSize, std::bit_ceil(size));
        desc.Usage = vk::BufferUsageFlagBits::eTransferSrc;
        desc.CpuWritable = true;
        desc.Name = "UploadStaging";

        StagingBlock& block = m_Blocks.emplace_back();
        block.Buffer = GpuBuffer(m_Context, desc);
        return block;
    }

    void UploadQueue::Upload(vk::Buffer destination, uint64_t destinationOffset, std::span<const std::byte> data)
    {
        if (data.empty())
            return;

        StagingBlock& block = BlockWithRoomFor(data.size());
        std::memcpy(static_cast<std::byte*>(block.Buffer.Data()) + block.Used, data.data(), data.size());

        m_Copies.push_back({ block.Buffer.Handle(), destination, vk::BufferCopy(block.Used, destinationOffset, data.size()) });
        block.Used = NextPlace(block.Used + data.size());
        block.HasPendingData = true;
    }

    void UploadQueue::UploadTexture(const GpuTexture& texture, std::span<const std::byte> pixels)
    {
        if (pixels.empty())
            return;

        StagingBlock& block = BlockWithRoomFor(pixels.size());
        std::memcpy(static_cast<std::byte*>(block.Buffer.Data()) + block.Used, pixels.data(), pixels.size());

        TextureCopy& copy = m_TextureCopies.emplace_back();
        copy.Staging = block.Buffer.Handle();
        copy.StagingOffset = block.Used;
        copy.Image = texture.Image();
        copy.Extent = vk::Extent2D(texture.Desc().Width, texture.Desc().Height);
        copy.MipLevels = texture.Desc().MipLevels;

        block.Used = NextPlace(block.Used + pixels.size());
        block.HasPendingData = true;
    }

    void UploadQueue::MoveContents(GpuBuffer source, vk::Buffer destination)
    {
        for (Copy& copy : m_Copies)
        {
            if (copy.Destination == source.Handle())
                copy.Destination = destination;
        }

        m_Moves.push_back({ std::move(source), destination });
    }

    void UploadQueue::Record(vk::CommandBuffer commands, const TextureCopy& copy) const
    {
        const auto levels = [](uint32_t first, uint32_t count) { return vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, first, count, 0, 1); };
        const auto level = [](uint32_t mip) { return vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, mip, 0, 1); };

        const auto transition = [&](uint32_t firstMip, uint32_t mipCount, vk::ImageLayout from, vk::ImageLayout to,
            vk::AccessFlags sourceAccess, vk::AccessFlags targetAccess, vk::PipelineStageFlags targetStage)
        {
            const vk::ImageMemoryBarrier barrier(sourceAccess, targetAccess, from, to, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, copy.Image, levels(firstMip, mipCount));
            const vk::PipelineStageFlags sourceStage = from == vk::ImageLayout::eUndefined ? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eTransfer;
            commands.pipelineBarrier(sourceStage, targetStage, {}, nullptr, nullptr, barrier);
        };

        // The texture is new, so nothing it held has to be kept.
        transition(0, copy.MipLevels, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            {}, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer);

        const vk::BufferImageCopy region(copy.StagingOffset, 0, 0, level(0), vk::Offset3D(0, 0, 0), vk::Extent3D(copy.Extent, 1));
        commands.copyBufferToImage(copy.Staging, copy.Image, vk::ImageLayout::eTransferDstOptimal, region);

        // Every smaller size is the one above it, scaled down by half with its pixels averaged.
        vk::Extent2D sourceExtent = copy.Extent;
        for (uint32_t mip = 1; mip < copy.MipLevels; ++mip)
        {
            const vk::Extent2D targetExtent(std::max(1u, sourceExtent.width / 2), std::max(1u, sourceExtent.height / 2));

            transition(mip - 1, 1, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead, vk::PipelineStageFlagBits::eTransfer);

            vk::ImageBlit blit;
            blit.srcSubresource = level(mip - 1);
            blit.srcOffsets[1] = vk::Offset3D(static_cast<int32_t>(sourceExtent.width), static_cast<int32_t>(sourceExtent.height), 1);
            blit.dstSubresource = level(mip);
            blit.dstOffsets[1] = vk::Offset3D(static_cast<int32_t>(targetExtent.width), static_cast<int32_t>(targetExtent.height), 1);
            commands.blitImage(copy.Image, vk::ImageLayout::eTransferSrcOptimal, copy.Image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

            sourceExtent = targetExtent;
        }

        // All sizes but the smallest were read from last, the smallest was written last.
        const vk::PipelineStageFlags readers = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader;
        if (copy.MipLevels > 1)
        {
            transition(0, copy.MipLevels - 1, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead, readers);
        }
        transition(copy.MipLevels - 1, 1, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, readers);
    }

    void UploadQueue::Flush(vk::CommandBuffer commands)
    {
        if (!HasPending())
            return;

        for (const TextureCopy& copy : m_TextureCopies)
            Record(commands, copy);
        m_TextureCopies.clear();

        if (!m_Moves.empty())
        {
            // A move reads what earlier copies of this frame may have written, and the uploads below
            // write into what a move has just written. The barriers keep the three in that order.
            const vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite);
            const auto wait = [&] { commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, barrier, nullptr, nullptr); };

            wait();
            for (const Move& move : m_Moves)
            {
                commands.copyBuffer(move.Source.Handle(), move.Destination, vk::BufferCopy(0, 0, move.Source.Size()));
                wait();
            }

            // The sources are given up here and destroyed a few frames later, after the copies have run.
            m_Moves.clear();
        }

        if (!m_Copies.empty())
        {
            for (const Copy& copy : m_Copies)
                commands.copyBuffer(copy.Staging, copy.Destination, copy.Region);

            // Whatever is drawn or computed next may read what was just written, and a compute shader
            // may write over it, as culling does with the draw commands.
            const vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite);
            commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader, {}, barrier, nullptr, nullptr);
        }

        m_Copies.clear();
        for (StagingBlock& block : m_Blocks)
        {
            if (block.HasPendingData)
            {
                block.HasPendingData = false;
                block.FlushedInFrame = m_Frame;
            }
        }
    }
}
