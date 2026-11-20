#include "CompositePass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/SamplerCache.h"
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
        };

        // NVRHI hands out the Vulkan object behind one of its own as an untyped pointer.
        template<typename Handle>
        Handle Native(nvrhi::Object object)
        {
            return Handle(static_cast<typename Handle::NativeType>(object.pointer));
        }
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

        const nvrhi::SamplerHandle linearSampler = SamplerCache::Get()->GetSampler(SamplerSettings());
        m_LinearSampler = ctx.Shared->SamplerIndex(Native<vk::Sampler>(linearSampler->getNativeObject(nvrhi::ObjectTypes::VK_Sampler)));

        m_PipelineState.SetShader("Shaders/Composite.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void CompositePass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Composite.slang"))
            return;
        if (program->PushConstantSize != sizeof(CompositePushData))
        {
            LX_CORE_ERROR("Composite.slang no longer has the constants the composite pass expects.");
            return;
        }

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

    void CompositePass::UseInput(Input& input, nvrhi::ITexture* texture)
    {
        if (input.Texture == texture)
            return;

        // The inputs change when the picture is resized. The old one keeps its place in the table
        // until the frames that read it are done, and is kept alive by NVRHI for as long.
        if (input.Texture)
            m_Vulkan->Shared->ReleaseTexture(input.Index);

        input.Texture = texture;
        input.Index = m_Vulkan->Shared->AddTexture(Native<vk::ImageView>(texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView)));
    }

    bool CompositePass::Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // The shader has never compiled. The error is in the log, and the output stays as it was.
        if (!m_Vulkan->Pipeline)
            return false;

        nvrhi::ITexture* sceneColor = renderData.SceneColorInput;
        nvrhi::ITexture* bloom = renderData.BloomTexture ? renderData.BloomTexture.Get() : ctx.BlackTexture.Get();
        UseInput(m_SceneColor, sceneColor);
        UseInput(m_Bloom, bloom);

        resources.Sample(sceneColor);
        resources.Sample(bloom);
        resources.ColorTarget = renderData.OutputFramebuffer->getDesc().colorAttachments[0].texture;

        // The triangle covers every pixel.
        resources.OverwritesTarget = true;
        return true;
    }

    void CompositePass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);

        CompositePushData push{};
        push.BloomIntensity = renderData.BloomIntensity;
        push.FXAAEnabled = renderData.FXAAEnabled ? 1 : 0;
        push.SceneTexture = m_SceneColor.Index;
        push.BloomTexture = m_Bloom.Index;
        push.LinearSampler = m_LinearSampler;
        m_Vulkan->Shared->Push(commands, push);

        commands.draw(3, 1, 0, 0);
    }
}
