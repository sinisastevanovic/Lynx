#include "GridPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/ShaderLayout.h"

namespace Lynx
{
    namespace
    {
        // Has to match GridConstants in Grid.slang.
        struct GridPushData
        {
            glm::mat4 InverseViewProjection;
        };
    }

    struct GridPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        vk::Device Device;

        // The buffer with the frame's camera, which NVRHI still owns.
        vk::Buffer SceneConstants;

        vk::DescriptorSetLayout SetLayout;
        vk::PipelineLayout PipelineLayout;
        vk::Pipeline Pipeline;
        vk::DescriptorPool DescriptorPool;
        vk::DescriptorSet DescriptorSet;
        vk::ShaderStageFlags PushConstantStages;

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

    GridPass::GridPass() = default;
    GridPass::~GridPass() = default;

    void GridPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Device = ctx.Graphics->Device();
        m_Vulkan->SceneConstants = vk::Buffer(static_cast<VkBuffer>(ctx.GlobalConstantBuffer->getNativeObject(nvrhi::ObjectTypes::VK_Buffer).pointer));

        m_PipelineState.SetShader("Shaders/Grid.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void GridPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        const std::optional<uint32_t> sceneBinding = FindBinding(*program, "scene");
        if (!sceneBinding || program->PushConstantSize != sizeof(GridPushData))
        {
            LX_CORE_ERROR("Grid.slang no longer has the parameters or constants the grid pass expects.");
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

        m_Vulkan->SetLayout = CreateSetLayout(device, *program, 0);
        m_Vulkan->PipelineLayout = CreatePipelineLayout(device, *program, m_Vulkan->SetLayout);
        m_Vulkan->PushConstantStages = ToVulkan(program->StageMask());

        // The camera buffer is the same object in every frame, so the set is written once.
        const vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, 1);
        m_Vulkan->DescriptorPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo({}, 1, poolSize));
        m_Vulkan->DescriptorSet = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_Vulkan->DescriptorPool, m_Vulkan->SetLayout)).front();

        const vk::DescriptorBufferInfo sceneInfo(m_Vulkan->SceneConstants, 0, VK_WHOLE_SIZE);
        device.updateDescriptorSets(vk::WriteDescriptorSet(m_Vulkan->DescriptorSet, *sceneBinding, 0, vk::DescriptorType::eUniformBuffer, {}, sceneInfo), nullptr);

        // The vertex shader makes two triangles that cover the screen from the vertex index alone.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->PipelineLayout;
        desc.Blend = BlendMode::Alpha;
        desc.ColorFormat = RenderFormats::Output;
        desc.DepthFormat = RenderFormats::SceneDepth;
        // Tested against the scene, but not written: the grid should not hide what is drawn after it.
        desc.DepthTest = true;
        desc.DepthWrite = false;
        m_Vulkan->Pipeline = CreateGraphicsPipeline(device, desc);
    }

    bool GridPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!renderData.ShowGrid || !m_Vulkan->Pipeline)
            return false;

        resources.ColorTarget = renderData.OutputFramebuffer->getDesc().colorAttachments[0].texture;
        resources.DepthTarget = renderData.TargetFramebuffer->getDesc().depthAttachment.texture;
        return true;
    }

    void GridPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);
        commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_Vulkan->PipelineLayout, 0, m_Vulkan->DescriptorSet, nullptr);

        GridPushData push;
        push.InverseViewProjection = glm::inverse(renderData.ViewProjection);
        commands.pushConstants(m_Vulkan->PipelineLayout, m_Vulkan->PushConstantStages, 0, sizeof(push), &push);

        commands.draw(6, 1, 0, 0);

        renderData.DrawCalls++;
        renderData.IndexCount += 6;
    }
}
