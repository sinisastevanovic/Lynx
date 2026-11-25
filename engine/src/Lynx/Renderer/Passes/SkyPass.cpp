#include "SkyPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match SkyConstants in Sky.slang.
        struct SkyPushData
        {
            glm::mat4 InverseViewProjection;
        };
    }

    struct SkyPass::VulkanObjects
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

    SkyPass::SkyPass() = default;
    SkyPass::~SkyPass() = default;

    void SkyPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/Sky.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void SkyPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Sky.slang", sizeof(SkyPushData)))
            return;

        // The vertex shader makes a triangle that covers the screen from the vertex index alone, at the
        // farthest depth. It passes where the depth buffer holds that depth too, which is where nothing is.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.ColorFormat = RenderFormats::SceneColor;
        desc.DepthFormat = RenderFormats::SceneDepth;
        desc.DepthTest = true;
        desc.DepthWrite = false;
        desc.DepthCompare = vk::CompareOp::eGreaterOrEqual;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool SkyPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // The sky's brightness means nothing in a view that shows colours as they are.
        if (!m_Vulkan->Pipeline || !renderData.AtmosphereTables.front() || ShowsRawColours(renderData.Viewing.Mode))
            return false;

        resources.ColorTarget = renderData.SceneColor;
        resources.DepthTarget = renderData.SceneDepth;
        for (GpuTexture* table : renderData.AtmosphereTables)
            resources.Sample(table);
        return true;
    }

    void SkyPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);

        SkyPushData push;
        push.InverseViewProjection = glm::inverse(renderData.ClipViewProjection);
        m_Vulkan->Shared->Push(commands, push);

        commands.draw(3, 1, 0, 0);
        renderData.DrawCalls++;
    }
}
