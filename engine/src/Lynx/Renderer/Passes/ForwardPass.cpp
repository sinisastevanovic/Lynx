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
            uint64_t VisibleInstances;
            uint64_t Materials;
            uint32_t ShadowMap;
            uint32_t ShadowSampler;
            ViewMode Mode;
            uint32_t ShowCutOuts;
        };
    }

    struct ForwardPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        vk::Pipeline OpaquePipeline;
        vk::Pipeline TransparentPipeline;

        // For the wireframe view: the edges of the triangles, of everything, see-through or not.
        vk::Pipeline WireframePipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(OpaquePipeline);
            Device.destroyPipeline(TransparentPipeline);
            Device.destroyPipeline(WireframePipeline);
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
            { MeshVertexLayout::PositionFormat, offsetof(PackedVertex, Position) },
            { MeshVertexLayout::NormalFormat, offsetof(PackedVertex, Normal) },
            { MeshVertexLayout::TangentFormat, offsetof(PackedVertex, Tangent) },
            { MeshVertexLayout::TexCoordFormat, offsetof(PackedVertex, TexCoord) },
        };
        desc.VertexStride = sizeof(PackedVertex);
        desc.DynamicCullMode = true;
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

        // The edges of the triangles. The depth pass has drawn the surfaces filled, so an edge is drawn where
        // it is not behind one, and edges of hidden triangles stay hidden. A line does not get exactly the
        // depth the filled triangle got, so it is pulled a little towards the camera.
        desc.Wireframe = true;
        desc.Blend = BlendMode::Opaque;
        desc.DepthCompare = vk::CompareOp::eGreaterOrEqual;
        desc.DepthBias = 4.0f;
        desc.DepthBiasSlope = 1.0f;
        const vk::Pipeline wireframePipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        // A reloaded shader replaces pipelines that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, oldOpaque = m_Vulkan->OpaquePipeline, oldTransparent = m_Vulkan->TransparentPipeline, oldWireframe = m_Vulkan->WireframePipeline]()
        {
            device.destroyPipeline(oldOpaque);
            device.destroyPipeline(oldTransparent);
            device.destroyPipeline(oldWireframe);
        });
        m_Vulkan->OpaquePipeline = opaquePipeline;
        m_Vulkan->TransparentPipeline = transparentPipeline;
        m_Vulkan->WireframePipeline = wireframePipeline;
    }

    bool ForwardPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipelines(*shader); });

        if (!m_Vulkan->OpaquePipeline || !m_Vulkan->TransparentPipeline)
            return false;
        const MeshDrawCommands& draws = renderData.MeshDraws;
        if (draws.Camera().Count == 0 && renderData.TransparentDraws.empty())
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
        // The wireframe view draws everything with one pipeline of its own.
        const bool isWireframe = renderData.Viewing.Mode == ViewMode::Wireframe && m_Vulkan->WireframePipeline;

        // Binding another pipeline leaves the bound buffers as they are, so once is enough for both pipelines.
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, isWireframe ? m_Vulkan->WireframePipeline : m_Vulkan->OpaquePipeline);
        renderData.Geometry->Bind(commands);

        // Push constants stay set across pipelines too, because all pipelines share one layout.
        StandardPushData push{};
        push.Instances = renderData.MeshInstanceAddress;
        push.VisibleInstances = renderData.VisibleInstanceAddress;
        push.Materials = renderData.MaterialAddress;
        push.ShadowMap = renderData.ShadowMapIndex;
        push.ShadowSampler = renderData.ShadowSamplerIndex;
        push.Mode = renderData.Viewing.Mode;
        push.ShowCutOuts = renderData.Viewing.ShowCutOuts ? 1u : 0u;
        m_Vulkan->Shared->Push(commands, push);

        // Opaque materials and those with cut-outs are shaded by the same pipeline, whichever phase of
        // culling found them, and the commands of the camera lie next to each other. So one call draws
        // as many of them as are drawn in the same state: everything, in a scene without two-sided materials.
        const MeshDrawCommands& draws = renderData.MeshDraws;
        DrawRange run;
        bool runIsTwoSided = false;
        const auto drawRun = [&]()
        {
            if (run.Count == 0)
                return;

            SetCulling(commands, runIsTwoSided);
            DrawMeshes(commands, draws, run);
            renderData.DrawCalls++;
            run = {};
        };

        for (const CullPhase phase : { CullPhase::Early, CullPhase::Late })
        {
            for (uint32_t index = 0; index < MeshDrawKind::Count; ++index)
            {
                // An empty range is skipped, which leaves no gap: the run goes on behind it.
                const MeshDrawKind kind = MeshDrawKind::At(index);
                const DrawRange range = draws.CameraOf(phase).Of(kind);
                if (range.Count == 0)
                    continue;

                if (kind.IsTwoSided != runIsTwoSided)
                    drawRun();
                if (run.Count == 0)
                {
                    run.First = range.First;
                    runIsTwoSided = kind.IsTwoSided;
                }
                run.Count += range.Count;
            }
        }
        drawRun();

        if (renderData.TransparentDraws.empty())
            return;

        if (!isWireframe)
            commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->TransparentPipeline);
        for (const TransparentDraw& draw : renderData.TransparentDraws)
        {
            SetCulling(commands, draw.IsTwoSided);
            DrawTransparent(commands, draw);
            renderData.DrawCalls++;
        }
    }
}
