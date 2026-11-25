#include "SkyLightPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/Vulkan/ComputePipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    struct SkyLightPass::VulkanObjects
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

    SkyLightPass::SkyLightPass() = default;
    SkyLightPass::~SkyLightPass() = default;

    void SkyLightPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/SkyLight.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void SkyLightPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The shader takes everything from the scene constants and has no push constants of its own.
        if (!m_Vulkan->Shared->Supports(*program, "SkyLight.slang", 0))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;

        // The new shader may give another result.
        m_Drawn.reset();
    }

    bool SkyLightPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!m_Vulkan->Pipeline || !renderData.HasSkyView)
            return false;

        // Still right from an earlier frame.
        if (m_Drawn == SkyInputsOf(renderData))
            return false;

        for (GpuTexture* table : renderData.AtmosphereTables)
            resources.Sample(table);
        resources.Computes = true;
        return true;
    }

    void SkyLightPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);
        commands.dispatch(1, 1, 1);
        m_Drawn = SkyInputsOf(renderData);
    }
}
