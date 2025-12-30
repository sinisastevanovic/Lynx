#pragma once
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    class ShadowPass : public RenderPass
    {
    public:
        ShadowPass(uint32_t resolution = 4096);
        virtual ~ShadowPass() = default;

        void Init(RenderContext& ctx) override;
        void Execute(RenderContext& ctx, RenderData& renderData) override;

        nvrhi::TextureHandle GetShadowMap() const { return m_ShadowMap; }
        nvrhi::SamplerHandle GetShadowSampler() const { return m_ShadowSampler; }

    private:
        nvrhi::BindingSetHandle GetMaskedBindingSet(RenderContext& ctx, RenderData& renderData, Material* material);
        void CreateGlobalBindingSet(RenderContext& ctx, RenderData& renderData);

    private:
        uint32_t m_Resolution;

        nvrhi::TextureHandle m_ShadowMap;
        nvrhi::FramebufferHandle m_Framebuffer;
        nvrhi::SamplerHandle m_ShadowSampler;

        nvrhi::BufferHandle m_ShadowConstantBuffer;
        nvrhi::BufferHandle m_CachedInstanceBuffer;

        nvrhi::BindingLayoutHandle m_GlobalBindingLayout;
        nvrhi::BindingLayoutHandle m_MaterialBindingLayout;
        nvrhi::GraphicsPipelineHandle m_Pipeline;

        nvrhi::BindingSetHandle m_GlobalBindingSet;
        nvrhi::BindingSetHandle m_OpaqueBindingSet;
        std::unordered_map<Material*, nvrhi::BindingSetHandle> m_MaskedBindingSets;
    };
}

