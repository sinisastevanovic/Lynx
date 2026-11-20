#include "DebugPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuBuffer.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/ShaderLayout.h"
#include "Lynx/Renderer/Vulkan/UploadQueue.h"

namespace Lynx
{
    struct DebugPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        UploadQueue* Uploads = nullptr;
        vk::Device Device;

        // The buffer with the frame's camera, which NVRHI still owns.
        vk::Buffer SceneConstants;

        vk::DescriptorSetLayout SetLayout;
        vk::PipelineLayout PipelineLayout;
        vk::Pipeline Pipeline;
        vk::DescriptorPool DescriptorPool;
        vk::DescriptorSet DescriptorSet;

        GpuBuffer VertexBuffer;

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

    DebugPass::DebugPass() = default;
    DebugPass::~DebugPass() = default;

    void DebugPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Uploads = ctx.Uploads;
        m_Vulkan->Device = ctx.Graphics->Device();
        m_Vulkan->SceneConstants = vk::Buffer(static_cast<VkBuffer>(ctx.GlobalConstantBuffer->getNativeObject(nvrhi::ObjectTypes::VK_Buffer).pointer));

        m_PipelineState.SetShader("Shaders/DebugLine.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void DebugPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        const std::optional<uint32_t> sceneBinding = FindBinding(*program, "scene");
        if (!sceneBinding)
        {
            LX_CORE_ERROR("DebugLine.slang no longer reads the scene constants the debug pass provides.");
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

        // The camera buffer is the same object in every frame, so the set is written once.
        const vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, 1);
        m_Vulkan->DescriptorPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo({}, 1, poolSize));
        m_Vulkan->DescriptorSet = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_Vulkan->DescriptorPool, m_Vulkan->SetLayout)).front();

        const vk::DescriptorBufferInfo sceneInfo(m_Vulkan->SceneConstants, 0, VK_WHOLE_SIZE);
        device.updateDescriptorSets(vk::WriteDescriptorSet(m_Vulkan->DescriptorSet, *sceneBinding, 0, vk::DescriptorType::eUniformBuffer, {}, sceneInfo), nullptr);

        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->PipelineLayout;
        desc.VertexAttributes = {
            { vk::Format::eR32G32B32Sfloat, offsetof(LineVertex, Position) },
            { vk::Format::eR32G32B32A32Sfloat, offsetof(LineVertex, Color) },
        };
        desc.VertexStride = sizeof(LineVertex);
        desc.Topology = vk::PrimitiveTopology::eLineList;
        desc.ColorFormat = RenderFormats::Output;
        desc.DepthFormat = RenderFormats::SceneDepth;
        // Tested against the scene, but not written: a line should not hide what is drawn after it.
        desc.DepthTest = true;
        desc.DepthWrite = false;
        m_Vulkan->Pipeline = CreateGraphicsPipeline(device, desc);
    }

    bool DebugPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (renderData.DebugLines.empty() || !m_Vulkan->Pipeline)
            return false;

        m_Vertices.clear();
        m_Vertices.reserve(renderData.DebugLines.size() * 2);
        for (const DebugLine& line : renderData.DebugLines)
        {
            m_Vertices.push_back({ line.Start, line.Color });
            m_Vertices.push_back({ line.End, line.Color });
        }

        const std::span<const std::byte> bytes = std::as_bytes(std::span<const LineVertex>(m_Vertices));
        if (m_Vulkan->VertexBuffer.Size() < bytes.size())
        {
            // Twice what is needed, so a few more lines every frame do not mean a new buffer every frame.
            GpuBufferDesc desc;
            desc.Size = bytes.size() * 2;
            desc.Usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
            desc.Name = "DebugLines";
            m_Vulkan->VertexBuffer = GpuBuffer(*m_Vulkan->Context, desc);
        }
        m_Vulkan->Uploads->Upload(m_Vulkan->VertexBuffer.Handle(), 0, bytes);

        resources.ColorTarget = renderData.OutputFramebuffer->getDesc().colorAttachments[0].texture;
        resources.DepthTarget = renderData.TargetFramebuffer->getDesc().depthAttachment.texture;
        return true;
    }

    void DebugPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);
        commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_Vulkan->PipelineLayout, 0, m_Vulkan->DescriptorSet, nullptr);
        commands.bindVertexBuffers(0, m_Vulkan->VertexBuffer.Handle(), vk::DeviceSize(0));
        commands.draw(static_cast<uint32_t>(m_Vertices.size()), 1, 0, 0);

        renderData.DrawCalls++;
        renderData.IndexCount += static_cast<uint32_t>(m_Vertices.size());
    }
}
