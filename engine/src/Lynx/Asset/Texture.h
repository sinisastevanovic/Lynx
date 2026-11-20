#pragma once
#include "Lynx/Core.h"
#include "Asset.h"
#include <nvrhi/nvrhi.h>

#include "TextureSpecification.h"
#include "Lynx/Renderer/TextureSlot.h"

namespace Lynx
{
    class LX_API Texture : public Asset
    {
    public:
        Texture(const std::string& filepath, const TextureSpecification& spec);
        Texture(std::vector<uint8_t> data, uint32_t width, uint32_t height, const std::string& debugName = "");

        ~Texture() override;

        static AssetType GetStaticType() { return AssetType::Texture; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        nvrhi::TextureHandle GetTextureHandle() const { return m_TextureHandle; }

        // Where the renderer's texture and sampler tables have this texture and its sampler.
        // Valid once the texture handle is.
        uint32_t GetTableIndex() const { return m_TableSlot.Index(); }
        uint32_t GetSamplerIndex() const { return m_SamplerIndex; }

        uint32_t GetWidth() const { return m_Specification.Width; }
        uint32_t GetHeight() const { return m_Specification.Height; }

        void SetSpecification(const TextureSpecification& spec) { m_Specification = spec; }
        const TextureSpecification& GetSpecification() const { return m_Specification; }
        const SamplerSettings& GetSamplerSettings() const { return m_Specification.SamplerSettings; }

        virtual bool LoadSourceData() override;
        virtual bool CreateRenderResources() override;
        virtual bool Reload() override;

    private:
        TextureSpecification m_Specification;
        nvrhi::TextureHandle m_TextureHandle;
        TextureSlot m_TableSlot;
        uint32_t m_SamplerIndex = 0;

        unsigned char* m_PixelData = nullptr;
    };
}

