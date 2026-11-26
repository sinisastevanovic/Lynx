#include "TexturePreviewPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <algorithm>

namespace Lynx
{
    namespace
    {
        // What the picture holds goes to the screen as it is.
        constexpr vk::Format PictureFormat = vk::Format::eR8G8B8A8Unorm;

        // A window is never larger than this. A request for more is a mistake, and is cut down.
        constexpr uint32_t LargestPicture = 8192;

        // Have to match TexturePreview.slang.
        constexpr uint32_t ShowsRed = 1;
        constexpr uint32_t ShowsGreen = 2;
        constexpr uint32_t ShowsBlue = 4;
        constexpr uint32_t ShowsAlpha = 8;

        // Has to match PreviewConstants in TexturePreview.slang.
        struct PreviewPushData
        {
            glm::vec2 UvStart;
            glm::vec2 UvSpan;
            uint32_t Texture;
            uint32_t Sampler;
            float Size;
            uint32_t Channels;
        };
    }

    struct TexturePreviewPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // One sampler that blends between texels, and one that shows each texel as the square it is.
        uint32_t BlendingSampler = 0;
        uint32_t NearestSampler = 0;

        GpuTexture Picture;
        TextureSlot PictureSlot;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    TexturePreviewPass::TexturePreviewPass() = default;
    TexturePreviewPass::~TexturePreviewPass() = default;

    void TexturePreviewPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        // Nothing repeats: what lies past the texture's edge is not part of it.
        SamplerSettings sampler;
        sampler.WrapMode = TextureWrap::Clamp;
        sampler.UseAnisotropy = false;
        sampler.FilterMode = TextureFilter::Bilinear;
        m_Vulkan->BlendingSampler = ctx.Shared->SamplerIndex(SamplerCache::Get()->GetSampler(sampler));
        sampler.FilterMode = TextureFilter::Nearest;
        m_Vulkan->NearestSampler = ctx.Shared->SamplerIndex(SamplerCache::Get()->GetSampler(sampler));

        m_PipelineState.SetShader("Shaders/TexturePreview.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void TexturePreviewPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "TexturePreview.slang", sizeof(PreviewPushData)))
            return;

        // The vertex shader makes a triangle that covers the picture from the vertex index alone.
        GraphicsPipelineDesc desc;
        desc.Program = program.get();
        desc.Layout = m_Vulkan->Shared->PipelineLayout();
        desc.ColorFormat = PictureFormat;
        const vk::Pipeline pipeline = CreateGraphicsPipeline(m_Vulkan->Device, desc);
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool TexturePreviewPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        if (!renderData.Preview || !m_Vulkan->Pipeline)
            return false;

        const uint32_t width = std::clamp(renderData.Preview->Width, 1u, LargestPicture);
        const uint32_t height = std::clamp(renderData.Preview->Height, 1u, LargestPicture);
        VulkanObjects& vulkan = *m_Vulkan;
        if (!vulkan.Picture || vulkan.Picture.Desc().Width != width || vulkan.Picture.Desc().Height != height)
        {
            GpuTextureDesc desc;
            desc.Width = width;
            desc.Height = height;
            desc.Format = PictureFormat;
            desc.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
            desc.Name = "TexturePreview";

            // The place in the table goes before the picture it points at.
            vulkan.PictureSlot = TextureSlot();
            vulkan.Picture = GpuTexture(*vulkan.Context, desc);
            vulkan.PictureSlot = TextureSlot(*vulkan.Shared, vulkan.Shared->AddTexture(vulkan.Picture.View()));
        }

        // The texture that is shown needs no mention: a texture of an asset is always ready for reading.
        resources.ColorTarget = &vulkan.Picture;
        resources.OverwritesTarget = true;
        return true;
    }

    void TexturePreviewPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        const TexturePreview& preview = *renderData.Preview;

        PreviewPushData push{};
        push.UvStart = preview.UvStart;
        push.UvSpan = preview.UvSpan;
        push.Texture = preview.TextureIndex;
        push.Sampler = preview.BlendsBetweenTexels ? m_Vulkan->BlendingSampler : m_Vulkan->NearestSampler;
        push.Size = preview.Size;
        push.Channels = (preview.ShowsRed ? ShowsRed : 0) | (preview.ShowsGreen ? ShowsGreen : 0) | (preview.ShowsBlue ? ShowsBlue : 0) | (preview.ShowsAlpha ? ShowsAlpha : 0);

        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Vulkan->Pipeline);
        m_Vulkan->Shared->Push(commands, push);
        commands.draw(3, 1, 0, 0);
        renderData.DrawCalls++;

        renderData.PreviewPicture = &m_Vulkan->Picture;
        renderData.PreviewPictureIndex = m_Vulkan->PictureSlot.Index();
    }
}
