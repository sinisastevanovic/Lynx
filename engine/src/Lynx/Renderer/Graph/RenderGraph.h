#pragma once

#include "Lynx/Renderer/RenderPass.h"

#include <vulkan/vulkan.hpp>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Lynx
{
    class FrameProfiler;
    class GpuTimers;

    // What a pass that draws with Vulkan directly needs from a frame. The pass states it,
    // and the render graph makes it so before the pass records a single command.
    struct PassResources
    {
        static constexpr uint32_t MaxSampled = 8;

        // Textures the pass reads in its shaders.
        std::array<nvrhi::ITexture*, MaxSampled> Sampled{};
        uint32_t SampledCount = 0;

        // The texture the pass draws into.
        nvrhi::ITexture* ColorTarget = nullptr;

        // The pass writes every pixel of the target, so what the target held before need not be kept.
        bool OverwritesTarget = false;

        void Sample(nvrhi::ITexture* texture) { Sampled[SampledCount++] = texture; }
    };

    // A pass that draws with Vulkan directly. It never changes the layout of a texture, never begins
    // or ends rendering and never sets the viewport: it declares what it needs, and records its draws.
    class VulkanPass
    {
    public:
        virtual ~VulkanPass() = default;

        virtual void Init(RenderContext& ctx) = 0;

        // Called every frame before Execute. Returns false if the pass has nothing to draw in this frame.
        virtual bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) = 0;

        // Called with rendering into the declared target already begun, and viewport and scissor set to cover it.
        virtual void Execute(vk::CommandBuffer commands, const RenderData& renderData) = 0;
    };

    // The passes of a frame, in the order they run.
    //
    // The graph measures every pass. For a pass that draws with Vulkan directly it also prepares what the
    // pass declared: textures are brought into the right layouts and rendering into the target is begun and ended.
    // Passes that still draw through NVRHI are run as they are, because NVRHI does those things for them.
    class RenderGraph
    {
    public:
        RenderGraph(FrameProfiler& profiler, GpuTimers& timers);
        ~RenderGraph();

        // The name identifies the pass in profiler output. A pass with a condition runs only in frames where it holds.
        RenderPass& Add(std::string name, std::unique_ptr<RenderPass> pass, std::function<bool()> condition = {});
        VulkanPass& Add(std::string name, std::unique_ptr<VulkanPass> pass, std::function<bool()> condition = {});

        void Init(RenderContext& ctx);
        void Execute(RenderContext& ctx, RenderData& renderData);

        // Destroys the passes.
        void Clear();

    private:
        struct Entry
        {
            std::string Name;
            std::unique_ptr<RenderPass> NvrhiPass;
            std::unique_ptr<VulkanPass> DirectPass;
            std::function<bool()> Condition;
        };

        void ExecuteDirectPass(VulkanPass& pass, RenderContext& ctx, const RenderData& renderData);

    private:
        FrameProfiler& m_Profiler;
        GpuTimers& m_Timers;
        std::vector<Entry> m_Passes;
    };
}
