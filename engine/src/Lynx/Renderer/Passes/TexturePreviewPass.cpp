#include "TexturePreviewPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/SamplerCache.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"
#include "Lynx/Renderer/Vulkan/GraphicsPipeline.h"
#include "Lynx/Renderer/Vulkan/Rendering.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <algorithm>
#include <unordered_map>

namespace Lynx
{
    namespace
    {
        // What the picture holds goes to the screen as it is.
        constexpr vk::Format PictureFormat = vk::Format::eR8G8B8A8Unorm;

        // A window is never larger than this. A request for more is a mistake, and is cut down.
        constexpr uint32_t LargestPicture = 8192;

        // A picture nobody asked for in this many frames belongs to a window that was closed.
        constexpr uint64_t FramesKeptUnused = 8;

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

        // The picture of every key that was asked for lately.
        struct Picture
        {
            GpuTexture Texture;
            TextureSlot Slot;
            uint64_t UsedInFrame = 0;
        };
        std::unordered_map<uint64_t, Picture> Pictures;
        uint64_t Frame = 0;

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

        VulkanObjects& vulkan = *m_Vulkan;
        ++vulkan.Frame;
        std::erase_if(vulkan.Pictures, [&vulkan](const auto& entry) { return vulkan.Frame - entry.second.UsedInFrame > FramesKeptUnused; });

        if (renderData.Previews.empty() || !vulkan.Pipeline)
            return false;

        for (const TexturePreviewRequest& request : renderData.Previews)
        {
            const uint32_t width = std::clamp(request.Preview.Width, 1u, LargestPicture);
            const uint32_t height = std::clamp(request.Preview.Height, 1u, LargestPicture);

            VulkanObjects::Picture& picture = vulkan.Pictures[request.Key];
            picture.UsedInFrame = vulkan.Frame;
            if (picture.Texture && picture.Texture.Desc().Width == width && picture.Texture.Desc().Height == height)
                continue;

            GpuTextureDesc desc;
            desc.Width = width;
            desc.Height = height;
            desc.Format = PictureFormat;
            desc.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
            desc.Name = "TexturePreview";

            // The place in the table goes before the picture it points at.
            picture.Slot = TextureSlot();
            picture.Texture = GpuTexture(*vulkan.Context, desc);
            picture.Slot = TextureSlot(*vulkan.Shared, vulkan.Shared->AddTexture(picture.Texture.View()));
        }

        // The pictures are this pass's own, several of them, and so are their layouts. The textures that
        // are shown need no mention: a texture of an asset is always ready for reading.
        resources.OwnTargets = true;
        return true;
    }

    void TexturePreviewPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        VulkanObjects& vulkan = *m_Vulkan;
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, vulkan.Pipeline);

        for (const TexturePreviewRequest& request : renderData.Previews)
        {
            const TexturePreview& preview = request.Preview;
            VulkanObjects::Picture& picture = vulkan.Pictures.at(request.Key);

            PreviewPushData push{};
            push.UvStart = preview.UvStart;
            push.UvSpan = preview.UvSpan;
            push.Texture = preview.TextureIndex;
            push.Sampler = preview.BlendsBetweenTexels ? vulkan.BlendingSampler : vulkan.NearestSampler;
            push.Size = preview.Size;
            push.Channels = (preview.ShowsRed ? ShowsRed : 0) | (preview.ShowsGreen ? ShowsGreen : 0) | (preview.ShowsBlue ? ShowsBlue : 0) | (preview.ShowsAlpha ? ShowsAlpha : 0);

            // Every pixel is drawn, so what the picture held does not matter.
            picture.Texture.Transition(commands, vk::ImageLayout::eColorAttachmentOptimal);
            RenderingTargets targets;
            targets.Color = picture.Texture.View();
            targets.Extent = vk::Extent2D(picture.Texture.Desc().Width, picture.Texture.Desc().Height);
            targets.ColorLoad = vk::AttachmentLoadOp::eDontCare;
            BeginRendering(commands, targets);

            vulkan.Shared->Push(commands, push);
            commands.draw(3, 1, 0, 0);
            commands.endRendering();
            renderData.DrawCalls++;

            // A window shows it later in the frame, so it is left ready for reading.
            picture.Texture.Transition(commands, vk::ImageLayout::eShaderReadOnlyOptimal);
            renderData.PreviewPictures.push_back({ request.Key, picture.Slot.Index() });
        }
    }
}
