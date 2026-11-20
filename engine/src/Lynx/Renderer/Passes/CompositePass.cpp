#include "CompositePass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/SamplerCache.h"

#include <vulkan/vulkan.hpp>

namespace Lynx
{
    namespace
    {
        struct CompositePushData
        {
            float BloomIntensity;
            int FXAAEnabled;
            float Padding[2];
        };

        // The format of every picture this pass writes: the window's images and the editor's viewport.
        constexpr vk::Format TargetFormat = vk::Format::eB8G8R8A8Unorm;

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

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
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
        const vk::Device device = m_Vulkan->Device;

        // Matches set 0 of Composite.glsl: the scene, a sampler, the bloom.
        const vk::DescriptorSetLayoutBinding bindings[] = {
            { 0, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment },
            { 1, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment },
            { 2, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment },
        };
        m_Vulkan->SetLayout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, bindings));

        const vk::PushConstantRange pushRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(CompositePushData));
        m_Vulkan->PipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo({}, m_Vulkan->SetLayout, pushRange));

        m_PipelineState.SetShader("Shaders/Composite.glsl");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void CompositePass::CreatePipeline(const Shader& shader)
    {
        const vk::Device device = m_Vulkan->Device;

        // A hot-reloaded shader replaces a pipeline that frames still on the GPU may be using.
        if (m_Vulkan->Pipeline)
            m_Vulkan->Context->Deletions().Retire([device, pipeline = m_Vulkan->Pipeline]() { device.destroyPipeline(pipeline); });

        // A pipeline keeps nothing of the shader modules it was made from, so NVRHI stays their only owner.
        const vk::PipelineShaderStageCreateInfo stages[] = {
            { {}, vk::ShaderStageFlagBits::eVertex, Native<vk::ShaderModule>(shader.GetVertexShader()->getNativeObject(nvrhi::ObjectTypes::VK_ShaderModule)), "main" },
            { {}, vk::ShaderStageFlagBits::eFragment, Native<vk::ShaderModule>(shader.GetPixelShader()->getNativeObject(nvrhi::ObjectTypes::VK_ShaderModule)), "main" },
        };

        // The vertex shader makes a triangle that covers the screen from the vertex index alone.
        const vk::PipelineVertexInputStateCreateInfo vertexInput;
        const vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);

        // Viewport and scissor follow the target's size, which changes with the window.
        const vk::PipelineViewportStateCreateInfo viewport({}, 1, nullptr, 1, nullptr);
        const vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        const vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

        vk::PipelineRasterizationStateCreateInfo rasterization;
        rasterization.polygonMode = vk::PolygonMode::eFill;
        rasterization.cullMode = vk::CullModeFlagBits::eNone;
        rasterization.lineWidth = 1.0f;

        const vk::PipelineMultisampleStateCreateInfo multisample({}, vk::SampleCountFlagBits::e1);
        const vk::PipelineDepthStencilStateCreateInfo depthStencil;

        vk::PipelineColorBlendAttachmentState blendAttachment;
        blendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        const vk::PipelineColorBlendStateCreateInfo blend({}, VK_FALSE, vk::LogicOp::eCopy, blendAttachment);

        // No render pass object: the pass names its target when it starts rendering.
        const vk::PipelineRenderingCreateInfo rendering(0, TargetFormat);

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.pNext = &rendering;
        pipelineInfo.setStages(stages);
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_Vulkan->PipelineLayout;

        m_Vulkan->Pipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;
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
            { m_Vulkan->DescriptorSet, 0, 0, vk::DescriptorType::eSampledImage, sceneInfo },
            { m_Vulkan->DescriptorSet, 1, 0, vk::DescriptorType::eSampler, samplerInfo },
            { m_Vulkan->DescriptorSet, 2, 0, vk::DescriptorType::eSampledImage, bloomInfo },
        };
        device.updateDescriptorSets(writes, nullptr);
    }

    void CompositePass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        nvrhi::ITexture* sceneColor = renderData.SceneColorInput;
        nvrhi::ITexture* bloom = renderData.BloomTexture ? renderData.BloomTexture.Get() : ctx.BlackTexture.Get();
        nvrhi::ITexture* target = renderData.TargetFramebuffer->getDesc().colorAttachments[0].texture;
        BindInputs(sceneColor, bloom);

        // The hand-over from NVRHI to Vulkan, in three steps.
        // 1. NVRHI tracks which layout every texture is in. It is asked to put the three textures into
        //    the layouts this pass needs, and they are left in those layouts, so its bookkeeping stays right.
        ctx.CommandList->setTextureState(sceneColor, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        ctx.CommandList->setTextureState(bloom, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        ctx.CommandList->setTextureState(target, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        ctx.CommandList->commitBarriers();

        // 2. NVRHI keeps a render pass open between draws and remembers what is bound. Clearing its state
        //    ends that render pass and makes the next NVRHI pass bind everything again.
        ctx.CommandList->clearState();

        // 3. From here on the commands go straight into the command buffer NVRHI is recording.
        const vk::CommandBuffer commands = Native<vk::CommandBuffer>(ctx.CommandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer));

        const nvrhi::TextureDesc& targetDesc = target->getDesc();
        const vk::Extent2D extent(targetDesc.width, targetDesc.height);
        const auto width = static_cast<float>(extent.width);
        const auto height = static_cast<float>(extent.height);

        // The triangle covers every pixel, so what the target held before does not matter.
        vk::RenderingAttachmentInfo colorAttachment;
        colorAttachment.imageView = Native<vk::ImageView>(target->getNativeView(nvrhi::ObjectTypes::VK_ImageView));
        colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

        commands.beginRendering(vk::RenderingInfo({}, vk::Rect2D({ 0, 0 }, extent), 1, 0, colorAttachment));

        // NVRHI draws with the viewport turned upside down, so that +Y is up as in Direct3D.
        // The shaders are written for that, so this pass does the same.
        commands.setViewport(0, vk::Viewport(0.0f, height, width, -height, 0.0f, 1.0f));
        commands.setScissor(0, vk::Rect2D({ 0, 0 }, extent));
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);
        commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_Vulkan->PipelineLayout, 0, m_Vulkan->DescriptorSet, nullptr);

        CompositePushData push{};
        push.BloomIntensity = renderData.BloomIntensity;
        push.FXAAEnabled = renderData.FXAAEnabled ? 1 : 0;
        commands.pushConstants(m_Vulkan->PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(push), &push);

        commands.draw(3, 1, 0, 0);
        commands.endRendering();
    }
}
