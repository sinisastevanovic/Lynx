#include "UploadQueue.h"

#include "DeletionQueue.h"
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
        block.Used += data.size();
        block.HasPendingData = true;
    }

    void UploadQueue::Flush(vk::CommandBuffer commands)
    {
        if (m_Copies.empty())
            return;

        for (const Copy& copy : m_Copies)
            commands.copyBuffer(copy.Staging, copy.Destination, copy.Region);

        // Whatever is drawn or computed next may read what was just written.
        const vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead);
        commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader, {}, barrier, nullptr, nullptr);

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
