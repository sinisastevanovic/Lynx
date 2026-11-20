#include "GridPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match GridConstants in Grid.slang.
        struct GridPushData
        {
            glm::mat4 InverseViewProjection;
        };
    }

    struct GridPass::VulkanObjects
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

    GridPass::GridPass() = default;
    GridPass::~GridPass() = default;

    void GridPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/Grid.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void GridPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Grid.slang", sizeof(GridPushData)))
            return;

        // The vertex shader makes two triangles that cover the screen from the vertex index alone.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.Blend = BlendMode::Alpha;
        desc.ColorFormat = RenderFormats::Output;
        desc.DepthFormat = RenderFormats::SceneDepth;
        // Tested against the scene, but not written: the grid should not hide what is drawn after it.
        desc.DepthTest = true;
        desc.DepthWrite = false;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool GridPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!renderData.ShowGrid || !m_Vulkan->Pipeline)
            return false;

        resources.ColorTarget = renderData.OutputFramebuffer->getDesc().colorAttachments[0].texture;
        resources.DepthTarget = renderData.TargetFramebuffer->getDesc().depthAttachment.texture;
        return true;
    }

    void GridPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);

        GridPushData push;
        push.InverseViewProjection = glm::inverse(renderData.ViewProjection);
        m_Vulkan->Shared->Push(commands, push);

        commands.draw(6, 1, 0, 0);

        renderData.DrawCalls++;
        renderData.IndexCount += 6;
    }
}
