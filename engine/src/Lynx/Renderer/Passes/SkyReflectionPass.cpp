#include "SkyReflectionPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/Rendering.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <algorithm>

namespace Lynx
{
    namespace
    {
        // Has to match ReflectionConstants in SkyReflection.slang.
        struct ReflectionPushData
        {
            float Roughness;
            float Size;
        };
    }

    struct SkyReflectionPass::VulkanObjects
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

    SkyReflectionPass::SkyReflectionPass() = default;
    SkyReflectionPass::~SkyReflectionPass() = default;

    void SkyReflectionPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/SkyReflection.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void SkyReflectionPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "SkyReflection.slang", sizeof(ReflectionPushData)))
            return;

        // The vertex shader makes a triangle that covers the target from the vertex index alone.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.ColorFormat = RenderFormats::AtmosphereTable;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;

        // The new shader may give another result.
        m_Drawn.reset();
    }

    bool SkyReflectionPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!m_Vulkan->Pipeline || !renderData.SkyReflection || !renderData.AtmosphereTables.front())
            return false;

        // Still right from an earlier frame.
        if (m_Drawn == SkyInputsOf(renderData))
            return false;

        for (GpuTexture* table : renderData.AtmosphereTables)
            resources.Sample(table);

        // Every size of the picture is drawn into on its own.
        resources.OwnTargets = true;
        return true;
    }

    void SkyReflectionPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        GpuTexture& picture = *renderData.SkyReflection;
        const uint32_t mipCount = picture.Desc().MipLevels;

        // No size reads another: all of them are worked out from the sky itself.
        picture.Transition(commands, vk::ImageLayout::eColorAttachmentOptimal);
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);

        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            const uint32_t size = std::max(1u, picture.Desc().Width >> mip);

            // Every pixel is drawn, so what the size held does not matter.
            RenderingTargets targets;
            targets.Color = picture.MipView(mip);
            targets.Extent = vk::Extent2D(size, size);
            targets.ColorLoad = vk::AttachmentLoadOp::eDontCare;
            BeginRendering(commands, targets);

            // The sizes share the roughness out evenly, from a mirror to fully rough.
            ReflectionPushData push;
            push.Roughness = mipCount > 1 ? static_cast<float>(mip) / static_cast<float>(mipCount - 1) : 0.0f;
            push.Size = static_cast<float>(size);
            m_Vulkan->Shared->Push(commands, push);
            commands.draw(3, 1, 0, 0);
            commands.endRendering();
        }

        picture.Transition(commands, vk::ImageLayout::eShaderReadOnlyOptimal);
        m_Drawn = SkyInputsOf(renderData);
    }
}
