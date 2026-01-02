#pragma once
#include "Lynx/Renderer/RenderPass.h"
#include "nvrhi/nvrhi.h"

namespace Lynx
{
    class MipMapBlitPass
    {
    public:
        MipMapBlitPass() = default;
        virtual ~MipMapBlitPass() = default;
        
        void Init(nvrhi::DeviceHandle device);
        void Generate(nvrhi::CommandListHandle commandList, nvrhi::TextureHandle texture);

    private:
        nvrhi::GraphicsPipelineHandle GetCachedPipeline(nvrhi::Format format);

    private:
        nvrhi::DeviceHandle m_Device;

        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::SamplerHandle m_Sampler;
        PipelineState m_PipelineState;

        std::unordered_map<nvrhi::Format, nvrhi::GraphicsPipelineHandle> m_PipelineCache;
    };
}

