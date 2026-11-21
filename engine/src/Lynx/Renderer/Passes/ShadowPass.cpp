#include "ShadowPass.h"

#include "MeshDraw.h"
#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <limits>

namespace Lynx
{
    namespace
    {
        // Has to match ShadowConstants in Shadow.slang.
        struct ShadowPushData
        {
            glm::mat4 LightViewProjection;
            uint64_t Instances;
            uint64_t Materials;
        };
    }

    struct ShadowPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        // Opaque materials need no fragment shader. Materials with cut-outs read their texture in one.
        vk::Pipeline OpaquePipeline;
        vk::Pipeline CutOutPipeline;

        GpuTexture ShadowMap;

        // Compares instead of reading, and treats everything outside the map as lit.
        vk::Sampler ShadowSampler;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(OpaquePipeline);
            Device.destroyPipeline(CutOutPipeline);
            Device.destroySampler(ShadowSampler);
        }
    };

    ShadowPass::ShadowPass(uint32_t resolution)
        : m_Resolution(resolution)
    {
    }

    ShadowPass::~ShadowPass() = default;

    void ShadowPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        GpuTextureDesc shadowDesc;
        shadowDesc.Width = m_Resolution;
        shadowDesc.Height = m_Resolution;
        shadowDesc.Format = RenderFormats::ShadowMap;
        shadowDesc.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        shadowDesc.Name = "ShadowMap";
        m_Vulkan->ShadowMap = GpuTexture(*ctx.Graphics, shadowDesc);

        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
        samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.compareOp = vk::CompareOp::eLess;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.maxLod = std::numeric_limits<float>::max();
        m_Vulkan->ShadowSampler = m_Vulkan->Device.createSampler(samplerInfo);

        m_ShadowMapIndex = ctx.Shared->AddTexture(m_Vulkan->ShadowMap.View());
        m_ShadowSamplerIndex = ctx.Shared->ComparisonSamplerIndex(m_Vulkan->ShadowSampler);

        m_PipelineState.SetShader("Shaders/Shadow.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void ShadowPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Shadow.slang", sizeof(ShadowPushData)))
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
        desc.CullMode = vk::CullModeFlagBits::eBack;
        desc.DepthFormat = RenderFormats::ShadowMap;
        desc.DepthTest = true;
        desc.DepthWrite = true;
        // The shadow map keeps the usual depth, 0 at the near plane, unlike the scene.
        desc.DepthCompare = vk::CompareOp::eLess;
        // Without the bias, a lit surface would shadow itself in stripes, because the map is coarser than the screen.
        desc.DepthBias = 1.0f;
        desc.DepthBiasSlope = 2.0f;
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

    bool ShadowPass::Declare(const RenderContext&, const RenderData&, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // The map is cleared even if nothing casts a shadow or the shader is broken:
        // the forward pass reads it either way, and an empty map means no shadows.
        resources.DepthTarget = &m_Vulkan->ShadowMap;
        resources.ClearsDepth = true;
        return true;
    }

    void ShadowPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        renderData.ShadowMap = &m_Vulkan->ShadowMap;
        renderData.ShadowMapIndex = m_ShadowMapIndex;
        renderData.ShadowSamplerIndex = m_ShadowSamplerIndex;

        const MeshDrawCommands& draws = renderData.MeshDraws;
        if (!m_Vulkan->OpaquePipeline || draws.ShadowOpaque.Count + draws.ShadowCutOut.Count == 0)
            return;

        // The pool and the push constants stay set when the pipeline changes.
        renderData.Geometry->Bind(commands);

        ShadowPushData push{};
        push.LightViewProjection = renderData.ClipLightViewProjection;
        push.Instances = renderData.MeshInstanceAddress;
        push.Materials = renderData.MaterialAddress;
        m_Vulkan->Shared->Push(commands, push);

        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->OpaquePipeline);
        DrawMeshes(commands, draws, draws.ShadowOpaque);

        if (draws.ShadowCutOut.Count > 0)
        {
            commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->CutOutPipeline);
            DrawMeshes(commands, draws, draws.ShadowCutOut);
        }
    }
}
