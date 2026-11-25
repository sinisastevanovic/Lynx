#include "CompositePass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match CompositeConstants in Composite.slang.
        struct CompositePushData
        {
            float BloomIntensity;
            int FXAAEnabled;
            uint32_t SceneTexture;
            uint32_t BloomTexture;
            uint32_t LinearSampler;
            int ToneMapped;
            uint32_t ShownInsteadTexture;
            int ShownInstead;
        };

        // Have to match Composite.slang.
        constexpr int ShowsMotion = 1;
        constexpr int ShowsAsItIs = 2;

    }

    struct CompositePass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    CompositePass::CompositePass() = default;
    CompositePass::~CompositePass() = default;

    void CompositePass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_LinearSampler = ctx.Shared->SamplerIndex(SamplerCache::Get()->GetSampler(SamplerSettings()));

        m_PipelineState.SetShader("Shaders/Composite.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void CompositePass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Composite.slang", sizeof(CompositePushData)))
            return;

        // The vertex shader makes a triangle that covers the screen from the vertex index alone, so there are no vertex attributes.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.ColorFormat = RenderFormats::Output;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool CompositePass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // The shader has never compiled. The error is in the log, and the output stays as it was.
        if (!m_Vulkan->Pipeline)
            return false;

        // The bloom needs no mention: the pass that made it left it ready for reading.
        resources.Sample(renderData.SceneColor);
        if (renderData.Viewing.Mode == ViewMode::Motion)
            resources.Sample(renderData.SceneMotion);
        else if (renderData.ShownInstead)
            resources.Sample(renderData.ShownInstead);
        resources.ColorTarget = renderData.Output;

        // The triangle covers every pixel.
        resources.OverwritesTarget = true;
        return true;
    }

    void CompositePass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);

        CompositePushData push{};
        push.BloomIntensity = renderData.Bloom.Enabled ? renderData.Bloom.Intensity : 0.0f;
        push.FXAAEnabled = renderData.FXAAEnabled ? 1 : 0;
        push.SceneTexture = renderData.SceneColorIndex;
        push.BloomTexture = renderData.BloomTextureIndex;
        push.LinearSampler = m_LinearSampler;
        push.ToneMapped = ShowsRawColours(renderData.Viewing.Mode) ? 0 : 1;
        if (renderData.Viewing.Mode == ViewMode::Motion)
        {
            push.ShownInsteadTexture = renderData.SceneMotionIndex;
            push.ShownInstead = ShowsMotion;
        }
        else if (renderData.ShownInstead)
        {
            push.ShownInsteadTexture = renderData.ShownInsteadIndex;
            push.ShownInstead = ShowsAsItIs;
        }
        m_Vulkan->Shared->Push(commands, push);

        commands.draw(3, 1, 0, 0);
    }
}
