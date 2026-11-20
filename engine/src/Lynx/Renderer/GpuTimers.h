#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Lynx
{
    class FrameProfiler;

    // Measures how long the GPU spends between two points of a frame's command buffer,
    // using Vulkan timestamp queries. Results are read a few frames later, when the GPU is
    // certain to have finished the frame, so reading never waits. Needs the hostQueryReset feature.
    class GpuTimers
    {
    public:
        GpuTimers(vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t queueFamily);
        ~GpuTimers();

        GpuTimers(const GpuTimers&) = delete;
        GpuTimers& operator=(const GpuTimers&) = delete;

        // Call once per frame before any Begin. Reports the times of the oldest measured frame.
        void BeginFrame(FrameProfiler& profiler);

        // A name may be measured once per frame. Timers may overlap and nest.
        void Begin(vk::CommandBuffer commandBuffer, std::string_view name);
        void End(vk::CommandBuffer commandBuffer, std::string_view name);

        // Reports every frame still outstanding. The device has to be idle.
        void Flush(FrameProfiler& profiler);

    private:
        // A frame's results are read when its slot comes up again, this many frames later.
        // It has to be more than the number of frames the CPU may be ahead of the GPU.
        static constexpr uint32_t SlotCount = 4;
        static constexpr uint32_t MaxTimersPerFrame = 32;
        static constexpr uint32_t QueriesPerSlot = MaxTimersPerFrame * 2;

        struct Slot
        {
            uint64_t Frame = 0;
            std::vector<std::string> Names;
        };

        void Report(uint32_t slotIndex, FrameProfiler& profiler);
        uint32_t FirstQuery(uint32_t slotIndex) const { return slotIndex * QueriesPerSlot; }

    private:
        vk::Device m_Device;
        vk::QueryPool m_QueryPool;
        double m_NanosecondsPerTick = 1.0;
        uint64_t m_TimestampMask = ~0ull;

        std::array<Slot, SlotCount> m_Slots;
        uint32_t m_CurrentSlot = 0;
    };
}
