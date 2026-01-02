#pragma once
#include "Lynx/Renderer/RenderPass.h"

namespace Lynx
{
    struct BloomPushData
    {
        glm::vec4 Params; // x: Threshold, y: Knee, z: Radius, w: Lod
        int Mode;
        float Padding[3];
    };

    struct BloomSettings
    {
        float Threshold = 1.0f;
        float Knee = 0.1f;
        float Radius = 1.0f;
        float Intensity = 0.04f;
        bool Enabled = true;
    };
    
    class BloomPass : public RenderPass
    {
    public:
        BloomPass();
        virtual ~BloomPass() = default;

        virtual void Init(RenderContext& ctx) override;
        virtual void Execute(RenderContext& ctx, RenderData& renderData) override;

        void EnsureResources(RenderContext& ctx, uint32_t width, uint32_t height);

        nvrhi::TextureHandle GetResult() const { return m_BloomTexture; }

        BloomSettings& GetSettings() { return m_Settings; }
        const BloomSettings& GetSettings() const { return m_Settings; }

    private:
        void CreatePipeline(RenderContext& ctx, std::shared_ptr<Shader> shader);

    private:
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::SamplerHandle m_Sampler;

        nvrhi::TextureHandle m_CachedInput;
        nvrhi::TextureHandle m_BloomTexture;

        nvrhi::BindingSetHandle m_InputBindingSet;
        std::vector<nvrhi::BindingSetHandle> m_BindingSets;
        std::vector<nvrhi::FramebufferHandle> m_Framebuffers;

        BloomSettings m_Settings;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        const uint32_t MAX_MIPS = 6;
        uint32_t m_MipCount = 0;

        PipelineState m_PipelineState;
    };

}
