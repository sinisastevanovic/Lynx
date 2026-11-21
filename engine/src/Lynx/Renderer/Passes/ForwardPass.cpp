#include "ForwardPass.h"

#include "MeshDraw.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match StandardConstants in StandardShading.slang.
        struct StandardPushData
        {
            glm::vec4 AlbedoColor;
            glm::vec4 EmissiveColorStrength;
            float Metallic;
            float Roughness;
            float AlphaCutoff;
            uint32_t AlbedoTexture;
            uint32_t NormalTexture;
            uint32_t MetallicRoughnessTexture;
            uint32_t EmissiveTexture;
            uint32_t MaterialSampler;
            uint32_t ShadowMap;
            uint32_t ShadowSampler;
            uint64_t Instances;
        };

        // Everything of a draw that comes from its material. The shadow map and the instances are added by the caller.
        StandardPushData MaterialConstants(const Material& material, const MaterialTextureIndices& textures)
        {
            StandardPushData push{};
            push.AlbedoColor = material.AlbedoColor;
            push.EmissiveColorStrength = glm::vec4(material.EmissiveColor, material.EmissiveStrength);
            push.Metallic = material.Metallic;
            push.Roughness = material.Roughness;
            push.AlphaCutoff = material.Mode == AlphaMode::Mask ? material.AlphaCutoff : -1.0f;
            push.AlbedoTexture = textures.Albedo;
            push.NormalTexture = textures.Normal;
            push.MetallicRoughnessTexture = textures.MetallicRoughness;
            push.EmissiveTexture = textures.Emissive;
            push.MaterialSampler = textures.Sampler;
            return push;
        }
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

        m_WritesEntityIds = ctx.PresentationFramebufferInfo.colorFormats.size() > 1;
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
        desc.FrontFace = vk::FrontFace::eCounterClockwise;
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
        desc.DepthCompare = vk::CompareOp::eLess;
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

        const nvrhi::FramebufferDesc& sceneTargets = renderData.TargetFramebuffer->getDesc();
        resources.ColorTarget = sceneTargets.colorAttachments[0].texture;
        if (m_WritesEntityIds)
            resources.IdTarget = sceneTargets.colorAttachments[1].texture;
        resources.DepthTarget = sceneTargets.depthAttachment.texture;
        resources.Sample(renderData.ShadowMap);
        return true;
    }

    void ForwardPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->OpaquePipeline);
        for (const BatchDrawCall& batch : renderData.OpaqueDrawCalls)
        {
            if (batch.InstanceCount == 0 || !(batch.Key.RenderFlags & RenderFlags::MainPass))
                continue;

            const Submesh& submesh = batch.Key.Mesh->GetSubmeshes()[batch.Key.SubmeshIndex];
            BindSubmesh(commands, submesh);

            StandardPushData push = MaterialConstants(*submesh.Material, batch.Textures);
            push.ShadowMap = renderData.ShadowMapIndex;
            push.ShadowSampler = renderData.ShadowSamplerIndex;
            push.Instances = MeshInstanceAddress(renderData, batch.FirstInstance);
            m_Vulkan->Shared->Push(commands, push);

            commands.drawIndexed(submesh.IndexCount, batch.InstanceCount, 0, 0, 0);

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
            BindSubmesh(commands, submesh);

            StandardPushData push = MaterialConstants(*submesh.Material, command.Textures);
            push.ShadowMap = renderData.ShadowMapIndex;
            push.ShadowSampler = renderData.ShadowSamplerIndex;
            push.Instances = MeshInstanceAddress(renderData, static_cast<uint32_t>(command.InstanceOffset));
            m_Vulkan->Shared->Push(commands, push);

            commands.drawIndexed(submesh.IndexCount, 1, 0, 0, 0);

            renderData.DrawCalls++;
            renderData.IndexCount += submesh.IndexCount;
        }
    }
}
