#include "ShadowPass.h"

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
        // Has to match ShadowConstants in Shadow.slang.
        struct ShadowPushData
        {
            glm::mat4 LightViewProjection;
            float AlphaCutoff;
            uint32_t AlbedoTexture;
            uint32_t AlbedoSampler;
            uint32_t Padding;
            uint64_t Instances;
            uint64_t Padding2;
        };
    }

    struct ShadowPass::VulkanObjects
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

        const auto shadowDesc = nvrhi::TextureDesc()
            .setWidth(m_Resolution).setHeight(m_Resolution)
            .setFormat(nvrhi::Format::D32)
            .setIsRenderTarget(true)
            .setDebugName("ShadowMap")
            .setInitialState(nvrhi::ResourceStates::DepthWrite)
            .setKeepInitialState(true);
        m_ShadowMap = ctx.Device->createTexture(shadowDesc);

        // Compares instead of reading, and treats everything outside the map as lit.
        const auto samplerDesc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Border)
            .setBorderColor(nvrhi::Color(1.0f))
            .setReductionType(nvrhi::SamplerReductionType::Comparison);
        m_ShadowSampler = ctx.Device->createSampler(samplerDesc);

        m_ShadowMapIndex = ctx.Shared->AddTexture(vk::ImageView(static_cast<VkImageView>(m_ShadowMap->getNativeView(nvrhi::ObjectTypes::VK_ImageView).pointer)));
        m_ShadowSamplerIndex = ctx.Shared->ComparisonSamplerIndex(vk::Sampler(static_cast<VkSampler>(m_ShadowSampler->getNativeObject(nvrhi::ObjectTypes::VK_Sampler).pointer)));

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
            { MeshVertexLayout::PositionFormat, offsetof(Vertex, Position) },
            { MeshVertexLayout::TexCoordFormat, offsetof(Vertex, TexCoord) },
        };
        desc.VertexStride = sizeof(Vertex);
        desc.CullMode = vk::CullModeFlagBits::eBack;
        desc.FrontFace = vk::FrontFace::eCounterClockwise;
        desc.DepthFormat = RenderFormats::ShadowMap;
        desc.DepthTest = true;
        desc.DepthWrite = true;
        // Without the bias, a lit surface would shadow itself in stripes, because the map is coarser than the screen.
        desc.DepthBias = 1.0f;
        desc.DepthBiasSlope = 2.0f;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool ShadowPass::Declare(const RenderContext&, const RenderData&, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // The map is cleared even if nothing casts a shadow or the shader is broken:
        // the forward pass reads it either way, and an empty map means no shadows.
        resources.DepthTarget = m_ShadowMap;
        resources.ClearsDepth = true;
        return true;
    }

    void ShadowPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        renderData.ShadowMap = m_ShadowMap;
        renderData.ShadowMapIndex = m_ShadowMapIndex;
        renderData.ShadowSamplerIndex = m_ShadowSamplerIndex;

        if (!m_Vulkan->Pipeline)
            return;

        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);

        ShadowPushData push{};
        push.LightViewProjection = renderData.LightViewProj;

        for (const BatchDrawCall& batch : renderData.OpaqueDrawCalls)
        {
            if (batch.InstanceCount == 0 || !(batch.Key.RenderFlags & RenderFlags::ShadowPass))
                continue;

            const Submesh& submesh = batch.Key.Mesh->GetSubmeshes()[batch.Key.SubmeshIndex];
            BindSubmesh(commands, submesh);

            push.AlphaCutoff = submesh.Material->Mode == AlphaMode::Mask ? submesh.Material->AlphaCutoff : -1.0f;
            push.AlbedoTexture = batch.Textures.Albedo;
            push.AlbedoSampler = batch.Textures.Sampler;
            push.Instances = MeshInstanceAddress(renderData, batch.FirstInstance);
            m_Vulkan->Shared->Push(commands, push);

            commands.drawIndexed(submesh.IndexCount, batch.InstanceCount, 0, 0, 0);
        }
    }
}
