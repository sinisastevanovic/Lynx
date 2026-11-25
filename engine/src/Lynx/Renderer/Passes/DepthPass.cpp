#include "DepthPass.h"

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
        // Has to match DepthConstants in DepthOnly.slang.
        struct DepthPushData
        {
            uint64_t Instances;
            uint64_t Transforms;
            uint64_t VisibleInstances;
            uint64_t Materials;
        };
    }

    struct DepthPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        // Opaque materials need no fragment shader. Materials with cut-outs read their texture in one.
        vk::Pipeline OpaquePipeline;
        vk::Pipeline CutOutPipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(OpaquePipeline);
            Device.destroyPipeline(CutOutPipeline);
        }
    };

    DepthPass::DepthPass(CullPhase phase)
        : m_Phase(phase)
    {
    }

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
            { MeshVertexLayout::PositionFormat, offsetof(PackedVertex, Position) },
            { MeshVertexLayout::TexCoordFormat, offsetof(PackedVertex, TexCoord) },
        };
        desc.VertexStride = sizeof(PackedVertex);
        desc.DynamicCullMode = true;
        desc.DepthFormat = RenderFormats::SceneDepth;
        desc.DepthTest = true;
        desc.DepthWrite = true;
        const vk::Pipeline cutOutPipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        desc.UseFragmentStage = false;
        const vk::Pipeline opaquePipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!cutOutPipeline || !opaquePipeline)
        {
            m_Vulkan->Device.destroyPipeline(cutOutPipeline);
            m_Vulkan->Device.destroyPipeline(opaquePipeline);
            return;
        }

        // A reloaded shader replaces pipelines that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, oldOpaque = m_Vulkan->OpaquePipeline, oldCutOut = m_Vulkan->CutOutPipeline]()
        {
            device.destroyPipeline(oldOpaque);
            device.destroyPipeline(oldCutOut);
        });
        m_Vulkan->OpaquePipeline = opaquePipeline;
        m_Vulkan->CutOutPipeline = cutOutPipeline;
    }

    bool DepthPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (renderData.MeshDraws.CameraOf(m_Phase).All().Count == 0 || !m_Vulkan->OpaquePipeline)
            return false;

        resources.DepthTarget = renderData.SceneDepth;
        return true;
    }

    void DepthPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        renderData.Geometry->Bind(commands);

        DepthPushData push{};
        push.Instances = renderData.MeshInstanceAddress;
        push.Transforms = renderData.MeshTransformAddress;
        push.VisibleInstances = renderData.VisibleInstanceAddress;
        push.Materials = renderData.MaterialAddress;
        m_Vulkan->Shared->Push(commands, push);

        const MeshDrawCommands& draws = renderData.MeshDraws;
        DrawDepthOfMeshes(commands, draws, draws.CameraOf(m_Phase), m_Vulkan->OpaquePipeline, m_Vulkan->CutOutPipeline, renderData.Viewing.ShowCutOuts);
    }
}
