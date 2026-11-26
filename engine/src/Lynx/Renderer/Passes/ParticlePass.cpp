#include "ParticlePass.h"

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
        // Has to match ParticleConstants in Particle.slang.
        struct ParticlePushData
        {
            glm::vec4 AlbedoColor;
            glm::vec2 Tiling;
            float EmissiveStrength;
            uint32_t AlbedoTexture;
            uint64_t Particles;
            uint32_t AlbedoSampler;
            uint32_t Padding;
        };
    }

    struct ParticlePass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        // The same shader twice: one covers what is behind a particle by its alpha, the other adds to it.
        vk::Pipeline AlphaPipeline;
        vk::Pipeline AdditivePipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(AlphaPipeline);
            Device.destroyPipeline(AdditivePipeline);
        }
    };

    ParticlePass::ParticlePass() = default;
    ParticlePass::~ParticlePass() = default;

    void ParticlePass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/Particle.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipelines(*shader); });
    }

    void ParticlePass::CreatePipelines(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipelines that work are kept.
        if (!m_Vulkan->Shared->Supports(*program, "Particle.slang", sizeof(ParticlePushData)))
            return;

        // The vertex shader makes the square from the vertex index alone, and reads the particle from a buffer.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.ColorFormat = RenderFormats::SceneColor;
        desc.ReactiveFormat = RenderFormats::SceneReactive;
        desc.DepthFormat = RenderFormats::SceneDepth;
        // Tested against the scene, but not written: particles are see-through and must not hide each other.
        desc.DepthTest = true;
        desc.DepthWrite = false;

        desc.Blend = BlendMode::Alpha;
        const vk::Pipeline alphaPipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        desc.Blend = BlendMode::Additive;
        const vk::Pipeline additivePipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);

        // A reloaded shader replaces pipelines that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, oldAlpha = m_Vulkan->AlphaPipeline, oldAdditive = m_Vulkan->AdditivePipeline]()
        {
            device.destroyPipeline(oldAlpha);
            device.destroyPipeline(oldAdditive);
        });
        m_Vulkan->AlphaPipeline = alphaPipeline;
        m_Vulkan->AdditivePipeline = additivePipeline;
    }

    bool ParticlePass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipelines(*shader); });

        if (renderData.ParticleQueue.empty() || !m_Vulkan->AlphaPipeline || !m_Vulkan->AdditivePipeline)
            return false;

        // The particles go into the scene's picture. The entity ID picture next to it is left alone.
        // They have no motion in the picture of motion, so they mark the pixels they cover.
        resources.ColorTarget = renderData.SceneColor;
        resources.ReactiveTarget = renderData.SceneReactive;
        resources.DepthTarget = renderData.SceneDepth;
        return true;
    }

    void ParticlePass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        vk::Pipeline boundPipeline;
        renderData.WroteReactive = true;

        for (const ParticleBatch& batch : renderData.ParticleQueue)
        {
            const vk::Pipeline pipeline = batch.Material->Mode == AlphaMode::Additive ? m_Vulkan->AdditivePipeline : m_Vulkan->AlphaPipeline;
            if (pipeline != boundPipeline)
            {
                commands.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                boundPipeline = pipeline;
            }

            ParticlePushData push{};
            push.AlbedoColor = batch.Material->AlbedoColor;
            push.Tiling = batch.Material->Tiling;
            push.EmissiveStrength = batch.Material->EmissiveStrength;
            push.AlbedoTexture = batch.AlbedoTexture;
            push.AlbedoSampler = batch.AlbedoSampler;
            // The shader counts particles from the first one of this draw.
            push.Particles = renderData.ParticleInstanceAddress + batch.StartOffset * sizeof(ParticleInstanceData);
            m_Vulkan->Shared->Push(commands, push);

            commands.draw(6, batch.Count, 0, 0);
            renderData.DrawCalls++;
        }
    }
}
