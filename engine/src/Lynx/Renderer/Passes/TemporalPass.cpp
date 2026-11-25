#include "TemporalPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/RenderFormats.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/ComputePipeline.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <array>

namespace Lynx
{
    namespace
    {
        // Has to match Temporal.slang: the threads of a group a side.
        constexpr uint32_t ThreadsPerSide = 8;

        // Has to match TemporalConstants in Temporal.slang.
        struct TemporalPushData
        {
            glm::ivec2 Size;
            uint32_t SceneTexture;
            uint32_t HistoryTexture;
            uint32_t OutputTexture;
            float CurrentWeight;
        };
    }

    struct TemporalPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // The result of this frame and of the last, which change roles every frame. Each has a place
        // in the table shaders read from and one in the table compute shaders write into.
        std::array<GpuTexture, 2> History;
        std::array<TextureSlot, 2> ReadSlots;
        std::array<TextureSlot, 2> WriteSlots;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    TemporalPass::TemporalPass() = default;
    TemporalPass::~TemporalPass() = default;

    void TemporalPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/Temporal.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void TemporalPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Temporal.slang", sizeof(TemporalPushData)))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool TemporalPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!m_Vulkan->Pipeline || !renderData.SceneColor || !renderData.Temporal.Accumulates)
        {
            m_Accumulated = 0;
            return false;
        }

        // The two pictures are as large as the scene's, and made anew when that changes.
        const GpuTextureDesc& scene = renderData.SceneColor->Desc();
        const GpuTextureDesc& history = m_Vulkan->History[0].Desc();
        if (!m_Vulkan->History[0] || history.Width != scene.Width || history.Height != scene.Height)
        {
            GpuTextureDesc desc;
            desc.Width = scene.Width;
            desc.Height = scene.Height;
            desc.Format = RenderFormats::SceneColor;
            desc.Usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
            desc.Name = "TemporalHistory";
            for (uint32_t index = 0; index < 2; ++index)
            {
                m_Vulkan->History[index] = GpuTexture(*m_Vulkan->Context, desc);
                m_Vulkan->ReadSlots[index] = TextureSlot(*m_Vulkan->Shared, m_Vulkan->Shared->AddTexture(m_Vulkan->History[index].View()));
                m_Vulkan->WriteSlots[index] = TextureSlot(*m_Vulkan->Shared,
                    m_Vulkan->Shared->AddWritableTexture(m_Vulkan->History[index].View()), TextureTable::Writable);
            }
            m_Accumulated = 0;
        }

        // An average of frames that show different things is no picture of anything.
        if (!renderData.ViewIsStill)
            m_Accumulated = 0;

        m_Target = 1 - m_Target;
        resources.Sample(renderData.SceneColor);
        if (m_Accumulated > 0)
            resources.Sample(&m_Vulkan->History[1 - m_Target]);
        resources.Write(&m_Vulkan->History[m_Target]);
        resources.Computes = true;
        return true;
    }

    void TemporalPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        const GpuTextureDesc& scene = renderData.SceneColor->Desc();

        // Every frame counts the same in the average: the new one by its share of all of them so far.
        // Without a picture to go on from, the frame's own is read in its place and counts in full.
        TemporalPushData push{};
        push.Size = { scene.Width, scene.Height };
        push.SceneTexture = renderData.SceneColorIndex;
        push.HistoryTexture = m_Accumulated > 0 ? m_Vulkan->ReadSlots[1 - m_Target].Index() : renderData.SceneColorIndex;
        push.OutputTexture = m_Vulkan->WriteSlots[m_Target].Index();
        push.CurrentWeight = 1.0f / static_cast<float>(m_Accumulated + 1);

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);
        m_Vulkan->Shared->Push(commands, push);
        commands.dispatch(GroupCountFor(scene.Width, ThreadsPerSide), GroupCountFor(scene.Height, ThreadsPerSide), 1);
        ++m_Accumulated;

        // The passes after this one work with the result in place of the frame's own picture.
        renderData.SceneColor = &m_Vulkan->History[m_Target];
        renderData.SceneColorIndex = m_Vulkan->ReadSlots[m_Target].Index();
    }
}
