#include "DepthPass.h"

#include "MeshDraw.h"
#include "Lynx/Asset/Material.h"
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
        // Has to match DepthConstants in DepthOnly.slang.
        struct DepthPushData
        {
            float AlphaCutoff;
            uint32_t AlbedoTexture;
            uint32_t AlbedoSampler;
            uint32_t Padding;
            uint64_t Instances;
        };
    }

    struct DepthPass::VulkanObjects
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

    DepthPass::DepthPass() = default;
    DepthPass::~DepthPass() = default;

    void DepthPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/DepthOnly.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void DepthPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "DepthOnly.slang", sizeof(DepthPushData)))
            return;

        // No colour target: only depth is drawn. The texture coordinate is needed for materials with cut-outs.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.VertexAttributes = {
            { MeshVertexLayout::PositionFormat, offsetof(Vertex, Position) },
            { MeshVertexLayout::TexCoordFormat, offsetof(Vertex, TexCoord) },
        };
        desc.VertexStride = sizeof(Vertex);
        desc.CullMode = vk::CullModeFlagBits::eBack;
        desc.DepthFormat = RenderFormats::SceneDepth;
        desc.DepthTest = true;
        desc.DepthWrite = true;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool DepthPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (renderData.OpaqueDrawCalls.empty() || !m_Vulkan->Pipeline)
            return false;

        resources.DepthTarget = renderData.SceneDepth;
        return true;
    }

    void DepthPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);
        renderData.Geometry->Bind(commands);

        for (const BatchDrawCall& batch : renderData.OpaqueDrawCalls)
        {
            if (!(batch.Key.RenderFlags & RenderFlags::MainPass))
                continue;

            const Submesh& submesh = batch.Key.Mesh->GetSubmeshes()[batch.Key.SubmeshIndex];

            DepthPushData push{};
            push.AlphaCutoff = submesh.Material->Mode == AlphaMode::Mask ? submesh.Material->AlphaCutoff : -1.0f;
            push.AlbedoTexture = batch.Textures.Albedo;
            push.AlbedoSampler = batch.Textures.Sampler;
            push.Instances = MeshInstanceAddress(renderData, batch.FirstInstance);
            m_Vulkan->Shared->Push(commands, push);

            DrawSubmesh(commands, submesh, batch.InstanceCount);
        }
    }
}
