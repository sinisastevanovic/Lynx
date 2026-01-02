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
        nvrhi::DeviceHandle m_Device;

        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::SamplerHandle m_Sampler;
        PipelineState m_PipelineState;
    };
}

