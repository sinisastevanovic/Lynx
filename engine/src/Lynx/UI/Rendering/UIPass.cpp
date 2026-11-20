#include "UIPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Asset/Texture.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/Vulkan/GpuBuffer.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"
#include "Lynx/Renderer/Vulkan/UploadQueue.h"

namespace Lynx
{
    namespace
    {
        // Has to match UIConstants in UI.slang and UIText.slang.
        struct UIPushData
        {
            glm::vec2 ScreenSize;
            float PixelRange;
            uint32_t Texture;
            uint32_t Sampler;
            uint32_t Padding;
        };

        // Makes sure the buffer can hold the data, then queues the data for upload.
        void Upload(GraphicsContext& context, UploadQueue& uploads, GpuBuffer& buffer, std::span<const std::byte> bytes, vk::BufferUsageFlagBits usage, const char* name)
        {
            if (buffer.Size() < bytes.size())
            {
                // Twice what is needed, so a UI that grows a little every frame does not mean a new buffer every frame.
                GpuBufferDesc desc;
                desc.Size = bytes.size() * 2;
                desc.Usage = usage | vk::BufferUsageFlagBits::eTransferDst;
                desc.Name = name;
                buffer = GpuBuffer(context, desc);
            }
            uploads.Upload(buffer.Handle(), 0, bytes);
        }
    }

    struct UIPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        UploadQueue* Uploads = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        vk::Pipeline StandardPipeline;
        vk::Pipeline TextPipeline;

        GpuBuffer VertexBuffer;
        GpuBuffer IndexBuffer;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (!Device)
                return;

            Device.destroyPipeline(StandardPipeline);
            Device.destroyPipeline(TextPipeline);
        }
    };

    UIPass::UIPass() = default;
    UIPass::~UIPass() = default;

    void UIPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Uploads = ctx.Uploads;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_StandardPipelineState.SetShader("Shaders/UI.slang");
        m_StandardPipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader, UIBatchType::Standard); });

        m_TextPipelineState.SetShader("Shaders/UIText.slang");
        m_TextPipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader, UIBatchType::Text); });
    }

    void UIPass::CreatePipeline(const Shader& shader, UIBatchType type)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shaders are written together. If they stop agreeing, the pipeline that works is kept.
        const char* shaderName = type == UIBatchType::Text ? "UIText.slang" : "UI.slang";
        if (!m_Vulkan->Shared->Supports(*program, shaderName, sizeof(UIPushData)))
            return;

        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.VertexStride = sizeof(UIVertex);
        desc.Blend = BlendMode::Alpha;
        desc.ColorFormat = RenderFormats::Output;

        // Both shaders read the same vertices. Text also reads the outline.
        desc.VertexAttributes = {
            { vk::Format::eR32G32B32Sfloat, offsetof(UIVertex, Position) },
            { vk::Format::eR32G32Sfloat, offsetof(UIVertex, UV) },
            { vk::Format::eR8G8B8A8Unorm, offsetof(UIVertex, Color) },
        };
        if (type == UIBatchType::Text)
        {
            desc.VertexAttributes.push_back({ vk::Format::eR8G8B8A8Unorm, offsetof(UIVertex, OutlineColor) });
            desc.VertexAttributes.push_back({ vk::Format::eR32Sfloat, offsetof(UIVertex, OutlineWidth) });
        }
        desc.VertexAttributes.push_back({ vk::Format::eR32G32B32A32Sfloat, offsetof(UIVertex, ClipRect) });

        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        vk::Pipeline& current = type == UIBatchType::Text ? m_Vulkan->TextPipeline : m_Vulkan->StandardPipeline;
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = current]() { device.destroyPipeline(old); });
        current = pipeline;
    }

    bool UIPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_StandardPipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader, UIBatchType::Standard); });
        m_TextPipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader, UIBatchType::Text); });

        if (!m_Vulkan->StandardPipeline || !m_Vulkan->TextPipeline)
            return false;

        m_Batcher.Begin();
        for (const std::shared_ptr<UICanvas>& canvas : renderData.Canvases)
            m_Batcher.Submit(canvas);

        if (!m_Batcher.HasData())
            return false;

        Upload(*m_Vulkan->Context, *m_Vulkan->Uploads, m_Vulkan->VertexBuffer, std::as_bytes(m_Batcher.GetVertices()), vk::BufferUsageFlagBits::eVertexBuffer, "UIVertices");
        Upload(*m_Vulkan->Context, *m_Vulkan->Uploads, m_Vulkan->IndexBuffer, std::as_bytes(m_Batcher.GetIndices()), vk::BufferUsageFlagBits::eIndexBuffer, "UIIndices");

        resources.ColorTarget = renderData.OutputFramebuffer->getDesc().colorAttachments[0].texture;
        return true;
    }

    void UIPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        commands.bindVertexBuffers(0, m_Vulkan->VertexBuffer.Handle(), vk::DeviceSize(0));
        commands.bindIndexBuffer(m_Vulkan->IndexBuffer.Handle(), 0, vk::IndexType::eUint32);

        const nvrhi::FramebufferInfoEx& output = renderData.OutputFramebuffer->getFramebufferInfo();

        UIPushData push{};
        push.ScreenSize = { static_cast<float>(output.width), static_cast<float>(output.height) };

        vk::Pipeline boundPipeline;

        for (const UIBatch& batch : m_Batcher.GetBatches())
        {
            // A texture that is still loading has no picture yet. Its elements appear once it has one.
            if (!batch.Texture || !batch.Texture->GetTextureHandle())
                continue;

            const vk::Pipeline pipeline = batch.Type == UIBatchType::Text ? m_Vulkan->TextPipeline : m_Vulkan->StandardPipeline;
            if (pipeline != boundPipeline)
            {
                commands.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                boundPipeline = pipeline;
            }

            push.PixelRange = batch.PixelRange;
            push.Texture = batch.Texture->GetTableIndex();
            push.Sampler = batch.Texture->GetSamplerIndex();
            m_Vulkan->Shared->Push(commands, push);

            commands.drawIndexed(batch.IndexCount, 1, batch.IndexStart, 0, 0);

            renderData.DrawCalls++;
            renderData.IndexCount += batch.IndexCount;
        }
    }
}
