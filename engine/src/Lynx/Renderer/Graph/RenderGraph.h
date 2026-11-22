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
        std::array<GpuTexture*, MaxSampled> Sampled{};
        uint32_t SampledCount = 0;

        // The texture the pass draws into. Null for a pass that draws depth only.
        GpuTexture* ColorTarget = nullptr;

        // The picture of entity IDs the pass writes next to the colour. Null for none.
        GpuTexture* IdTarget = nullptr;

        // Set instead of a colour target by a pass that draws into pictures of its own, in several steps.
        // Such a pass begins and ends rendering itself (see Vulkan/Rendering.h) and looks after the layouts
        // of its own pictures. Everything else is still done for it.
        bool OwnTargets = false;

        // Set by a pass that draws nothing and runs compute shaders instead, on buffers it reaches through
        // their addresses. Nothing is rendered into. What the pass writes can be read by every pass after it.
        bool Computes = false;

        // The depth buffer the pass tests against, and writes if its pipeline says so. Null for none.
        GpuTexture* DepthTarget = nullptr;

        // The pass writes every pixel of the target, so what the target held before need not be kept.
        bool OverwritesTarget = false;

        // The depth buffer starts out empty, with everything farthest away, instead of keeping what it held.
        bool ClearsDepth = false;

        void Sample(GpuTexture* texture) { Sampled[SampledCount++] = texture; }
    };

    // A pass of the frame. It never changes the layout of a texture, never begins
    // or ends rendering, never sets the viewport and never binds a descriptor set: it declares what it needs,
    // and records its draws. Its shaders read the camera and all textures through the shared bindings.
    class VulkanPass
    {
    public:
        virtual ~VulkanPass() = default;

        virtual void Init(RenderContext& ctx) = 0;

        // Called every frame before Execute. Returns false if the pass has nothing to draw in this frame.
        virtual bool Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources) = 0;

        // Called with rendering into the declared targets already begun, viewport and scissor set to cover them,
        // and the shared bindings bound. A pass with targets of its own is called before rendering has begun.
        // A pass that computes is called with the shared bindings bound for compute: it binds its pipeline,
        // sets its push constants and dispatches.
        // What the pass queued for upload in Declare has arrived by then.
        virtual void Execute(vk::CommandBuffer commands, RenderData& renderData) = 0;
    };

    // The passes of a frame, in the order they run.
    //
    // The graph measures every pass and prepares what the pass declared: textures are brought into
    // the right layouts, and rendering into the targets is begun and ended.
    class RenderGraph
    {
    public:
        RenderGraph(FrameProfiler& profiler, GpuTimers& timers);
        ~RenderGraph();

        // The name identifies the pass in profiler output. A pass with a condition runs only in frames where it holds.
        VulkanPass& Add(std::string name, std::unique_ptr<VulkanPass> pass, std::function<bool()> condition = {});

        void Init(RenderContext& ctx);
        void Execute(RenderContext& ctx, RenderData& renderData);

        // Destroys the passes.
        void Clear();

    private:
        struct Entry
        {
            std::string Name;
            std::unique_ptr<VulkanPass> Pass;
            std::function<bool()> Condition;
        };

        void ExecutePass(VulkanPass& pass, RenderContext& ctx, RenderData& renderData);

    private:
        FrameProfiler& m_Profiler;
        GpuTimers& m_Timers;
        std::vector<Entry> m_Passes;
    };
}
