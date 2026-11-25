#include "AtmospherePass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/Rendering.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match TableConstants in AtmosphereTables.slang. The table is the number of an AtmosphereTable.
        struct TablePushData
        {
            uint32_t Table;
        };
    }

    struct AtmospherePass::VulkanObjects
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

    AtmospherePass::AtmospherePass() = default;
    AtmospherePass::~AtmospherePass() = default;

    void AtmospherePass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/AtmosphereTables.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void AtmospherePass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "AtmosphereTables.slang", sizeof(TablePushData)))
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

        // The new shader may fill the tables differently.
        m_Drawn.reset();
    }

    bool AtmospherePass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!m_Vulkan->Pipeline || !renderData.AtmosphereTables.front())
            return false;

        // The tables are drawn into one after the other, each reading the ones before it.
        resources.OwnTargets = true;
        return true;
    }

    void AtmospherePass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);

        const auto draw = [&](AtmosphereTable table)
        {
            GpuTexture& texture = *renderData.AtmosphereTables[static_cast<size_t>(table)];
            texture.Transition(commands, vk::ImageLayout::eColorAttachmentOptimal);

            // Every pixel is drawn, so what the table held does not matter.
            RenderingTargets targets;
            targets.Color = texture.View();
            targets.Extent = vk::Extent2D(texture.Desc().Width, texture.Desc().Height);
            targets.ColorLoad = vk::AttachmentLoadOp::eDontCare;
            BeginRendering(commands, targets);

            m_Vulkan->Shared->Push(commands, TablePushData{ static_cast<uint32_t>(table) });
            commands.draw(3, 1, 0, 0);
            commands.endRendering();

            // The next table and the scene's shaders read it.
            texture.Transition(commands, vk::ImageLayout::eShaderReadOnlyOptimal);
        };

        if (m_Drawn != renderData.Atmosphere)
        {
            draw(AtmosphereTable::Transmittance);
            draw(AtmosphereTable::MultiScattering);
            m_Drawn = renderData.Atmosphere;
        }
        draw(AtmosphereTable::SkyView);
        renderData.HasSkyView = true;
    }
}
