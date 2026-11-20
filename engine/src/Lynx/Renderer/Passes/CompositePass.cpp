#include "CompositePass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/ShaderLayout.h"

#include <vulkan/vulkan.hpp>

namespace Lynx
{
    namespace
    {
        // Has to match CompositeConstants in Composite.slang.
        struct CompositePushData
        {
            float BloomIntensity;
            int FXAAEnabled;
        };

        // NVRHI hands out the Vulkan object behind one of its own as an untyped pointer.
        template<typename Handle>
        Handle Native(nvrhi::Object object)
        {
            return Handle(static_cast<typename Handle::NativeType>(object.pointer));
        }
    }

    struct CompositePass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        vk::Device Device;
        vk::DescriptorSetLayout SetLayout;
        vk::PipelineLayout PipelineLayout;
        vk::Pipeline Pipeline;
        vk::DescriptorPool DescriptorPool;
        vk::DescriptorSet DescriptorSet;

        // Where the shader expects its inputs, read from the compiled program.
        uint32_t SceneTextureBinding = 0;
        uint32_t SamplerBinding = 0;
        uint32_t BloomTextureBinding = 0;
        vk::ShaderStageFlags PushConstantStages;

        // The renderer waitsfor the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(Pipeline);
            Device.destroyDescriptorPool(DescriptorPool);
            Device.destroyPipelineLayout(PipelineLayout);
            Device.destroyDescriptorSetLayout(SetLayout);
        }
    };

    CompositePass::CompositePass() = default;
    CompositePass::~CompositePass() = default;

    void CompositePass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Device = ctx.Graphics->Device();
        m_PipelineState.SetShader("Shaders/Composite.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void CompositePass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        const ShaderStageCode* vertexCode = program->FindStage(ShaderStage::Vertex);
        const ShaderStageCode* fragmentCode = program->FindStage(ShaderStage::Fragment);
        const std::optional<uint32_t> sceneTexture = FindBinding(*program, "sceneTexture");
        const std::optional<uint32_t> sampler = FindBinding(*program, "linearSampler");
        const std::optional<uint32_t> bloomTexture = FindBinding(*program, "bloomTexture");

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!vertexCode || !fragmentCode || !sceneTexture || !sampler || !bloomTexture || program->PushConstantSize != sizeof(CompositePushData))
        {
            LX_CORE_ERROR("Composite.slang no longer has the stages, parameters or constants the composite pass expects.");
            return;
        }

        // A reloaded shader replaces objects that frames still on the GPU may be using.
        const vk::Device device = m_Vulkan->Device;
        m_Vulkan->Context->Deletions().Retire([device, pipeline = m_Vulkan->Pipeline, pipelineLayout = m_Vulkan->PipelineLayout, setLayout = m_Vulkan->SetLayout, pool = m_Vulkan->DescriptorPool]()
        {
            device.destroyPipeline(pipeline);
            device.destroyPipelineLayout(pipelineLayout);
            device.destroyDescriptorPool(pool);
            device.destroyDescriptorSetLayout(setLayout);
        });

        // The layout is whatever the shader declares. Nothing about it is repeated here.
        m_Vulkan->SetLayout = CreateSetLayout(device, *program, 0);
        m_Vulkan->PipelineLayout = CreatePipelineLayout(device, *program, m_Vulkan->SetLayout);
        m_Vulkan->PushConstantStages = ToVulkan(program->StageMask());
        m_Vulkan->SceneTextureBinding = *sceneTexture;
        m_Vulkan->SamplerBinding = *sampler;
        m_Vulkan->BloomTextureBinding = *bloomTexture;

        // The descriptor set belonged to the old layout, so the inputs are bound again.
        m_Vulkan->DescriptorPool = nullptr;
        m_BoundSceneColor = nullptr;
        m_BoundBloom = nullptr;

        // The vertex shader makes a triangle that covers the screen from the vertex index alone, so there are no vertex attributes.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->PipelineLayout;
        desc.ColorFormat = RenderFormats::Output;
        m_Vulkan->Pipeline = CreateGraphicsPipeline(device, desc);
    }

    void CompositePass::BindInputs(nvrhi::ITexture* sceneColor, nvrhi::ITexture* bloom)
    {
        if (m_BoundSceneColor == sceneColor && m_BoundBloom == bloom)
            return;

        // The inputs change when the picture is resized. A descriptor set may not be rewritten while a frame
        // that uses it is still on the GPU, so a new one is made and the old one is given up.
        const vk::Device device = m_Vulkan->Device;
        if (m_Vulkan->DescriptorPool)
            m_Vulkan->Context->Deletions().Retire([device, pool = m_Vulkan->DescriptorPool]() { device.destroyDescriptorPool(pool); });

        const vk::DescriptorPoolSize poolSizes[] = {
            { vk::DescriptorType::eSampledImage, 2 },
            { vk::DescriptorType::eSampler, 1 },
        };
        m_Vulkan->DescriptorPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo({}, 1, poolSizes));
        m_Vulkan->DescriptorSet = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_Vulkan->DescriptorPool, m_Vulkan->SetLayout)).front();

        m_BoundSceneColor = sceneColor;
        m_BoundBloom = bloom;

        const vk::DescriptorImageInfo sceneInfo({}, Native<vk::ImageView>(sceneColor->getNativeView(nvrhi::ObjectTypes::VK_ImageView)), vk::ImageLayout::eShaderReadOnlyOptimal);
        const vk::DescriptorImageInfo bloomInfo({}, Native<vk::ImageView>(bloom->getNativeView(nvrhi::ObjectTypes::VK_ImageView)), vk::ImageLayout::eShaderReadOnlyOptimal);
        const vk::DescriptorImageInfo samplerInfo(Native<vk::Sampler>(SamplerCache::Get()->GetSampler(SamplerSettings())->getNativeObject(nvrhi::ObjectTypes::VK_Sampler)));

        const vk::WriteDescriptorSet writes[] = {
            { m_Vulkan->DescriptorSet, m_Vulkan->SceneTextureBinding, 0, vk::DescriptorType::eSampledImage, sceneInfo },
            { m_Vulkan->DescriptorSet, m_Vulkan->SamplerBinding, 0, vk::DescriptorType::eSampler, samplerInfo },
            { m_Vulkan->DescriptorSet, m_Vulkan->BloomTextureBinding, 0, vk::DescriptorType::eSampledImage, bloomInfo },
        };
        device.updateDescriptorSets(writes, nullptr);
    }

    bool CompositePass::Declare(const RenderContext& ctx, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // The shader has never compiled. The error is in the log, and the output stays as it was.
        if (!m_Vulkan->Pipeline)
            return false;

        nvrhi::ITexture* sceneColor = renderData.SceneColorInput;
        nvrhi::ITexture* bloom = renderData.BloomTexture ? renderData.BloomTexture.Get() : ctx.BlackTexture.Get();
        BindInputs(sceneColor, bloom);

        resources.Sample(sceneColor);
        resources.Sample(bloom);
        resources.ColorTarget = renderData.OutputFramebuffer->getDesc().colorAttachments[0].texture;

        // The triangle covers every pixel.
        resources.OverwritesTarget = true;
        return true;
    }

    void CompositePass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);
        commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_Vulkan->PipelineLayout, 0, m_Vulkan->DescriptorSet, nullptr);

        CompositePushData push{};
        push.BloomIntensity = renderData.BloomIntensity;
        push.FXAAEnabled = renderData.FXAAEnabled ? 1 : 0;
        commands.pushConstants(m_Vulkan->PipelineLayout, m_Vulkan->PushConstantStages, 0, sizeof(push), &push);

        commands.draw(3, 1, 0, 0);
    }
}
