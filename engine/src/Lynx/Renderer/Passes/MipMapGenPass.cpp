#include "MipMapGenPass.h"

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
    
    void MipMapGenPass::Init(nvrhi::DeviceHandle device)
    {
        m_Device = device;

        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/MipMapGen.glsl");

        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::Compute)
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1))
            .addItem(nvrhi::BindingLayoutItem::Texture_UAV(2).setSize(4))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(MipMapPushConstants)))
            .setBindingOffsets({0, 0, 0, 0});
        m_BindingLayout = m_Device->createBindingLayout(layoutDesc);

        auto computePipeDesc = nvrhi::ComputePipelineDesc()
            .addBindingLayout(m_BindingLayout)
            .setComputeShader(shader->GetComputeShader());
        m_Pipeline = m_Device->createComputePipeline(computePipeDesc);

        auto samplerDesc = nvrhi::SamplerDesc()
            .setAllFilters(true)
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_Sampler = m_Device->createSampler(samplerDesc);

        auto nullDesc = nvrhi::TextureDesc()
            .setWidth(1)
            .setHeight(1)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setIsUAV(true)
            .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
            .setKeepInitialState(true);
        m_NullTexture = m_Device->createTexture(nullDesc);
    }

    void MipMapGenPass::Generate(nvrhi::CommandListHandle commandList, nvrhi::TextureHandle texture)
    {
        const auto& desc = texture->getDesc();
        uint32_t mipLevels = desc.mipLevels;

        if (mipLevels <= 1)
            return;

        commandList->beginMarker("MipMapGen");

        // We generate mips in batches of 4
        // Dispatch 0: Reads Mip 0 -> Writes Mip 1, 2, 3, 4
        // Dispatch 1: Reads Mip 4 -> Writes Mip 5, 6, 7, 8
        for (uint32_t srcMip = 0; srcMip < mipLevels - 1; srcMip += 4)
        {
            uint32_t width = std::max(1u, desc.width >> srcMip);
            uint32_t height = std::max(1u, desc.height >> srcMip);

            uint32_t mipsRemaining = mipLevels - 1 - srcMip;
            uint32_t numLODs = std::min(4u, mipsRemaining);

            auto setDesc = nvrhi::BindingSetDesc()
                .addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture).setSubresources(nvrhi::TextureSubresourceSet(srcMip, 1, 0, 1)))
                .addItem(nvrhi::BindingSetItem::Sampler(1, m_Sampler));

            for (uint32_t i = 0; i < 4; ++i)
            {
                if (i < numLODs)
                {
                    setDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(2, texture)
                        .setArrayElement(i)
                        .setSubresources(nvrhi::TextureSubresourceSet(srcMip + 1 + i, 1, 0, 1)));
                }
                else
                {
                    setDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(2, m_NullTexture).setArrayElement(i));
                }
            }

            // TODO: Create binding set cache like donut if we actually generate mips every frame some time. For reflections for example.
            // NOTE: creating binding sets per frame is bad practice for performance.
            // Ideally cache these in a DescriptorTable or similar, but for asset loading (one-off), this is fine.
            auto bindingSet = m_Device->createBindingSet(setDesc, m_BindingLayout);

            MipMapPushConstants push;
            push.NumLODs = numLODs;
            push.TexSizeX = (float)width;
            push.TexSizeY = (float)height;

            nvrhi::ComputeState state;
            state.pipeline = m_Pipeline;
            state.bindings = { bindingSet };
            commandList->setComputeState(state);
            commandList->setPushConstants(&push, sizeof(push));

            uint32_t groupX = (width + 15) / 16;
            uint32_t groupY = (height + 15) / 16;
            commandList->dispatch(groupX, groupY, 1);

            if (srcMip + 4 < mipLevels - 1)
            {
                commandList->setEnableUavBarriersForTexture(texture, true);
            }
        }

        commandList->endMarker();
    }
}
