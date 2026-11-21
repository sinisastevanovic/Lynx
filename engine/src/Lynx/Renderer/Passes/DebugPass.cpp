#include "DebugPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuBuffer.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"
#include "Lynx/Renderer/Vulkan/UploadQueue.h"

namespace Lynx
{
    struct DebugPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        UploadQueue* Uploads = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        GpuBuffer VertexBuffer;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    DebugPass::DebugPass() = default;
    DebugPass::~DebugPass() = default;

    void DebugPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Uploads = ctx.Uploads;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/DebugLine.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void DebugPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // If the shader stops fitting the pass, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "DebugLine.slang", 0))
            return;

        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
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
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
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

        resources.ColorTarget = renderData.Output;
        resources.DepthTarget = renderData.SceneDepth;
        return true;
    }

    void DebugPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);
        commands.bindVertexBuffers(0, m_Vulkan->VertexBuffer.Handle(), vk::DeviceSize(0));
        commands.draw(static_cast<uint32_t>(m_Vertices.size()), 1, 0, 0);

        renderData.DrawCalls++;
        renderData.IndexCount += static_cast<uint32_t>(m_Vertices.size());
    }
}
