#pragma once
#include "Lynx/Renderer/RenderPass.h"
#include "nvrhi/nvrhi.h"

namespace Lynx
{
    class MipMapGenPass
    {
    public:
        MipMapGenPass() = default;
        virtual ~MipMapGenPass() = default;
        
        void Init(nvrhi::DeviceHandle device);
        void Generate(nvrhi::CommandListHandle commandList, nvrhi::TextureHandle texture);

    private:
        void CreatePipeline(std::shared_ptr<Shader> shader);
    private:
        nvrhi::DeviceHandle m_Device;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::ComputePipelineHandle m_Pipeline;
        nvrhi::SamplerHandle m_Sampler;

        // Dummy texture for unused UAV slots
        nvrhi::TextureHandle m_NullTexture;
        PipelineState m_PipelineState;
    };
}

