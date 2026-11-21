#include "ForwardPass.h"

#include "MeshDraw.h"
#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match StandardConstants in StandardShading.slang.
        struct StandardPushData
        {
            uint64_t Instances;
            uint64_t Materials;
            uint32_t ShadowMap;
            uint32_t ShadowSampler;
        };
    }

    struct ForwardPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        vk::Pipeline OpaquePipeline;
        vk::Pipeline TransparentPipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(OpaquePipeline);
            Device.destroyPipeline(TransparentPipeline);
        }
    };

    ForwardPass::ForwardPass() = default;
    ForwardPass::~ForwardPass() = default;

    void ForwardPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_WritesEntityIds = ctx.WritesEntityIds;
        m_PipelineState.SetShader(m_WritesEntityIds ? "Shaders/StandardEditor.slang" : "Shaders/Standard.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipelines(*shader); });
    }

    void ForwardPass::CreatePipelines(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipelines that work are kept.
        const char* shaderName = m_WritesEntityIds ? "StandardEditor.slang" : "Standard.slang";
        if (!m_Vulkan->Shared->Supports(*program, shaderName, sizeof(StandardPushData)))
            return;

        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.VertexAttributes = {
            { MeshVertexLayout::PositionFormat, offsetof(Vertex, Position) },
            { MeshVertexLayout::NormalFormat, offsetof(Vertex, Normal) },
            { MeshVertexLayout::TangentFormat, offsetof(Vertex, Tangent) },
            { MeshVertexLayout::TexCoordFormat, offsetof(Vertex, TexCoord) },
        };
        desc.VertexStride = sizeof(Vertex);
        desc.CullMode = vk::CullModeFlagBits::eBack;
        desc.ColorFormat = RenderFormats::SceneColor;
        if (m_WritesEntityIds)
            desc.IdFormat = RenderFormats::EntityIds;
        desc.DepthFormat = RenderFormats::SceneDepth;
        desc.DepthTest = true;
        desc.DepthWrite = false;

        // The depth pass has already written the depth of the nearest opaque surface at every pixel.
        // Drawing only where the depth is equal to it shades exactly that surface and nothing behind it.
        desc.DepthCompare = vk::CompareOp::eEqual;
        const vk::Pipeline opaquePipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        // See-through surfaces are not in the depth buffer. They are hidden by opaque ones, and cover what is behind them by their alpha.
        desc.DepthCompare = vk::CompareOp::eGreater;
        desc.Blend = BlendMode::Alpha;
        const vk::Pipeline transparentPipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        // A reloaded shader replaces pipelines that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, oldOpaque = m_Vulkan->OpaquePipeline, oldTransparent = m_Vulkan->TransparentPipeline]()
        {
            device.destroyPipeline(oldOpaque);
            device.destroyPipeline(oldTransparent);
        });
        m_Vulkan->OpaquePipeline = opaquePipeline;
        m_Vulkan->TransparentPipeline = transparentPipeline;
    }

    bool ForwardPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipelines(*shader); });

        if (!m_Vulkan->OpaquePipeline || !m_Vulkan->TransparentPipeline)
            return false;
        if (renderData.OpaqueDrawCalls.empty() && renderData.TransparentQueue.empty())
            return false;

        resources.ColorTarget = renderData.SceneColor;
        if (m_WritesEntityIds)
            resources.IdTarget = renderData.SceneEntityIds;
        resources.DepthTarget = renderData.SceneDepth;
        resources.Sample(renderData.ShadowMap);
        return true;
    }

    void ForwardPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        // Binding another pipeline leaves the bound buffers as they are, so once is enough for both pipelines.
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->OpaquePipeline);
        renderData.Geometry->Bind(commands);

        // Push constants stay set across pipelines too, because all pipelines share one layout.
        StandardPushData push{};
        push.Instances = renderData.MeshInstanceAddress;
        push.Materials = renderData.MaterialAddress;
        push.ShadowMap = renderData.ShadowMapIndex;
        push.ShadowSampler = renderData.ShadowSamplerIndex;
        m_Vulkan->Shared->Push(commands, push);

        for (const BatchDrawCall& batch : renderData.OpaqueDrawCalls)
        {
            if (batch.InstanceCount == 0 || !(batch.Key.RenderFlags & RenderFlags::MainPass))
                continue;

            const Submesh& submesh = batch.Key.Mesh->GetSubmeshes()[batch.Key.SubmeshIndex];
            DrawSubmesh(commands, submesh, batch.FirstInstance, batch.InstanceCount);

            renderData.DrawCalls++;
            renderData.IndexCount += submesh.IndexCount * batch.InstanceCount;
        }

        if (renderData.TransparentQueue.empty())
            return;

        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->TransparentPipeline);
        for (const RenderCommand& command : renderData.TransparentQueue)
        {
            if (!(command.Flags & RenderFlags::MainPass))
                continue;

            const Submesh& submesh = command.Mesh->GetSubmeshes()[command.SubmeshIndex];
            DrawSubmesh(commands, submesh, static_cast<uint32_t>(command.InstanceOffset), 1);

            renderData.DrawCalls++;
            renderData.IndexCount += submesh.IndexCount;
        }
    }
}
