#include "GpuTimers.h"

#include "Lynx/Log.h"
#include "Lynx/Profiling/FrameProfiler.h"

#include <algorithm>

namespace Lynx
{
    GpuTimers::GpuTimers(vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t queueFamily)
        : m_Device(device)
    {
        m_NanosecondsPerTick = physicalDevice.getProperties().limits.timestampPeriod;

        // Timestamps wrap at the number of bits the queue supports.
        const uint32_t validBits = physicalDevice.getQueueFamilyProperties().at(queueFamily).timestampValidBits;
        if (validBits == 0)
            LX_CORE_WARN("Queue family {0} does not support timestamps: GPU times will read as zero.", queueFamily);
        else if (validBits < 64)
            m_TimestampMask = (1ull << validBits) - 1;

        vk::QueryPoolCreateInfo poolInfo;
        poolInfo.queryType = vk::QueryType::eTimestamp;
        poolInfo.queryCount = SlotCount * QueriesPerSlot;
        m_QueryPool = m_Device.createQueryPool(poolInfo);
        m_Device.resetQueryPool(m_QueryPool, 0, poolInfo.queryCount);
    }

    GpuTimers::~GpuTimers()
    {
        m_Device.destroyQueryPool(m_QueryPool);
    }

    void GpuTimers::BeginFrame(FrameProfiler& profiler)
    {
        m_CurrentSlot = (m_CurrentSlot + 1) % SlotCount;
        Report(m_CurrentSlot, profiler);
        m_Slots[m_CurrentSlot].Frame = profiler.CurrentFrame();
    }

    void GpuTimers::Begin(vk::CommandBuffer commandBuffer, std::string_view name)
    {
        Slot& slot = m_Slots[m_CurrentSlot];
        if (slot.Names.size() >= MaxTimersPerFrame)
            return;

        const uint32_t query = FirstQuery(m_CurrentSlot) + static_cast<uint32_t>(slot.Names.size()) * 2;
        slot.Names.emplace_back(name);
        commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_QueryPool, query);
    }

    void GpuTimers::End(vk::CommandBuffer commandBuffer, std::string_view name)
    {
        const Slot& slot = m_Slots[m_CurrentSlot];
        const auto found = std::ranges::find(slot.Names, name);
        if (found == slot.Names.end())
            return;

        const uint32_t query = FirstQuery(m_CurrentSlot) + static_cast<uint32_t>(found - slot.Names.begin()) * 2 + 1;
        commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_QueryPool, query);
    }

    void GpuTimers::Flush(FrameProfiler& profiler)
    {
        for (uint32_t offset = 1; offset <= SlotCount; ++offset)
            Report((m_CurrentSlot + offset) % SlotCount, profiler);
    }

    void GpuTimers::Report(uint32_t slotIndex, FrameProfiler& profiler)
    {
        Slot& slot = m_Slots[slotIndex];
        if (slot.Names.empty())
            return;

        // Each query returns its value followed by a flag that says whether the value was written.
        struct Result
        {
            uint64_t Ticks;
            uint64_t Available;
        };
        std::array<Result, QueriesPerSlot> results{};

        const uint32_t queryCount = static_cast<uint32_t>(slot.Names.size()) * 2;
        const vk::Result status = m_Device.getQueryPoolResults(m_QueryPool, FirstQuery(slotIndex), queryCount,
            queryCount * sizeof(Result), results.data(), sizeof(Result),
            vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability);

        if (status == vk::Result::eSuccess || status == vk::Result::eNotReady)
        {
            for (size_t timer = 0; timer < slot.Names.size(); ++timer)
            {
                const Result& begin = results[timer * 2];
                const Result& end = results[timer * 2 + 1];
                if (begin.Available == 0 || end.Available == 0)
                    continue;

                const uint64_t ticks = (end.Ticks - begin.Ticks) & m_TimestampMask;
                profiler.AddGpuTime(slot.Frame, slot.Names[timer], static_cast<double>(ticks) * m_NanosecondsPerTick * 1e-6);
            }
        }

        m_Device.resetQueryPool(m_QueryPool, FirstQuery(slotIndex), QueriesPerSlot);
        slot.Names.clear();
    }
}
