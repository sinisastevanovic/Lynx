#include "TerrainPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuBuffer.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match TerrainConstants in Terrain.slang.
        struct TerrainPushData
        {
            uint64_t Patches;
            uint64_t Terrains;
            ViewMode Mode;
            uint32_t Padding;
        };
    }

    struct TerrainPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // The colour pass draws the edges of the triangles with this one in the wireframe view.
        vk::Pipeline WireframePipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(Pipeline);
            Device.destroyPipeline(WireframePipeline);
        }
    };

    TerrainPass::TerrainPass(TerrainPassKind kind)
        : m_Kind(kind)
    {
    }

    TerrainPass::~TerrainPass() = default;

    void TerrainPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/Terrain.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void TerrainPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Terrain.slang", sizeof(TerrainPushData)))
            return;

        // No vertex data: the shader works out where a vertex is from its index. Ground is seen from above only.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.CullMode = vk::CullModeFlagBits::eBack;
        desc.DepthFormat = RenderFormats::SceneDepth;
        desc.DepthTest = true;

        vk::Pipeline wireframePipeline;
        if (m_Kind == TerrainPassKind::Depth)
        {
            desc.DepthWrite = true;
            desc.UseFragmentStage = false;
        }
        else
        {
            // Only where the depth pass has drawn this very ground: nothing is shaded that ends up hidden.
            desc.ColorFormat = RenderFormats::SceneColor;
            desc.DepthWrite = false;
            desc.DepthCompare = vk::CompareOp::eEqual;

            // Edges lie on the surfaces they belong to, so they are pulled a little towards the camera.
            GraphicsPipelineDesc wireframeDesc = desc;
            wireframeDesc.Wireframe = true;
            wireframeDesc.DepthCompare = vk::CompareOp::eGreaterOrEqual;
            wireframeDesc.DepthBias = 4.0f;
            wireframeDesc.DepthBiasSlope = 1.0f;
            wireframePipeline = CreateGraphicsPipeline(m_Vulkan->Device, wireframeDesc);
        }

        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline || (m_Kind == TerrainPassKind::Colour && !wireframePipeline))
        {
            m_Vulkan->Device.destroyPipeline(pipeline);
            m_Vulkan->Device.destroyPipeline(wireframePipeline);
            return;
        }

        // A reloaded shader replaces pipelines that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline, oldWireframe = m_Vulkan->WireframePipeline]()
        {
            device.destroyPipeline(old);
            device.destroyPipeline(oldWireframe);
        });
        m_Vulkan->Pipeline = pipeline;
        m_Vulkan->WireframePipeline = wireframePipeline;
    }

    bool TerrainPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!m_Vulkan->Pipeline || renderData.Terrain.PatchCount == 0)
            return false;

        resources.DepthTarget = renderData.SceneDepth;
        if (m_Kind == TerrainPassKind::Colour)
        {
            // The same light as the meshes get: the sun's shadow, and the sky.
            resources.ColorTarget = renderData.SceneColor;
            for (GpuTexture* shadowMap : renderData.ShadowMaps)
                resources.Sample(shadowMap);
            if (GpuTexture* transmittance = renderData.AtmosphereTables[static_cast<size_t>(AtmosphereTable::Transmittance)])
                resources.Sample(transmittance);
            if (renderData.SkyReflection)
                resources.Sample(renderData.SkyReflection);
        }
        return true;
    }

    void TerrainPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        const TerrainDraws& terrain = renderData.Terrain;
        const bool isWireframe = m_Kind == TerrainPassKind::Colour && renderData.Viewing.Mode == ViewMode::Wireframe;

        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, isWireframe ? m_Vulkan->WireframePipeline : m_Vulkan->Pipeline);
        commands.bindIndexBuffer(terrain.Indices->Handle(), 0, vk::IndexType::eUint16);

        TerrainPushData push{};
        push.Patches = terrain.PatchAddress;
        push.Terrains = terrain.TerrainAddress;
        push.Mode = renderData.Viewing.Mode;
        m_Vulkan->Shared->Push(commands, push);

        for (const TerrainDraws::Run& run : terrain.Runs)
        {
            if (run.PatchCount == 0)
                continue;

            commands.drawIndexed(run.IndexCount, run.PatchCount, run.FirstIndex, 0, run.FirstPatch);
            renderData.DrawCalls++;
        }
    }
}
