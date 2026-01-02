#include "MipMapBlitPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    struct MipMapPushConstants
    {
        uint32_t NumLODs;
        float TexSizeX;
        float TexSizeY;
        float Padding;
    };
    
    void MipMapBlitPass::Init(nvrhi::DeviceHandle device)
    {
        m_Device = device;

        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::Pixel)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))
            .setBindingOffsets({0, 0, 0, 0});
        m_BindingLayout = m_Device->createBindingLayout(layoutDesc);
    }

    void MipMapBlitPass::Generate(nvrhi::CommandListHandle commandList, nvrhi::TextureHandle texture)
    {
        const auto& desc = texture->getDesc();
        if (desc.mipLevels <= 1) return;

        commandList->beginMarker("MipMapGen_Blit");

        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Blit.glsl");
        if (!m_BindingLayout) Init(m_Device); // Ensure layout exists

        auto pipeline = GetCachedPipeline(desc.format);

        // Sampler (Linear Clamp)
        if (!m_Sampler) {
             auto sDesc = nvrhi::SamplerDesc()
                .setAllFilters(true)
                .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
             m_Sampler = m_Device->createSampler(sDesc);
        }

        // Loop Mips
        for (uint32_t i = 1; i < desc.mipLevels; ++i)
        {
            uint32_t width = std::max(1u, desc.width >> i);
            uint32_t height = std::max(1u, desc.height >> i);

            // Output Framebuffer (Mip i)
            auto fbDesc = nvrhi::FramebufferDesc()
                .addColorAttachment(nvrhi::FramebufferAttachment(texture).setSubresources(nvrhi::TextureSubresourceSet(i, 1, 0, 1)));
            auto framebuffer = m_Device->createFramebuffer(fbDesc);

            // Input Binding Set (Mip i-1)
            auto setDesc = nvrhi::BindingSetDesc()
                .addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture).setSubresources(nvrhi::TextureSubresourceSet(i-1, 1, 0, 1)))
                .addItem(nvrhi::BindingSetItem::Sampler(1, m_Sampler));
            auto bindingSet = m_Device->createBindingSet(setDesc, m_BindingLayout);

            // Draw
            auto state = nvrhi::GraphicsState()
                .setPipeline(pipeline)
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(width, height)));

            commandList->setGraphicsState(state);
            commandList->draw(nvrhi::DrawArguments().setVertexCount(3));
        }

        commandList->endMarker();
    }

    nvrhi::GraphicsPipelineHandle MipMapBlitPass::GetCachedPipeline(nvrhi::Format format)
    {
        if (m_PipelineCache.contains(format))
            return m_PipelineCache[format];

        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/Blit.glsl");

        nvrhi::FramebufferInfo fbInfo;
        fbInfo.addColorFormat(format); // The key variation

        auto pipeDesc = nvrhi::GraphicsPipelineDesc()
            .addBindingLayout(m_BindingLayout)
            .setVertexShader(shader->GetVertexShader())
            .setFragmentShader(shader->GetPixelShader())
            .setPrimType(nvrhi::PrimitiveType::TriangleList);
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipeDesc.renderState.depthStencilState.depthTestEnable = false;
        pipeDesc.renderState.blendState.targets[0].setBlendEnable(false);

        auto pipeline = m_Device->createGraphicsPipeline(pipeDesc, fbInfo);
        m_PipelineCache[format] = pipeline;

        return pipeline;
    }
}
