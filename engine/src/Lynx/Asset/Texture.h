#pragma once
#include "Lynx/Core.h"
#include "Asset.h"
#include <nvrhi/nvrhi.h>

#include "TextureSpecification.h"

namespace Lynx
{
    class LX_API Texture : public Asset
    {
    public:
        Texture(const std::string& filepath);
        Texture(std::vector<uint8_t> data, uint32_t width, uint32_t height, const std::string& debugName = "");
        virtual ~Texture() = default;

        static AssetType GetStaticType() { return AssetType::Texture; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        nvrhi::TextureHandle GetTextureHandle() const { return m_TextureHandle; }

        uint32_t GetWidth() const { return m_Specification.Width; }
        uint32_t GetHeight() const { return m_Specification.Height; }
        const std::string& GetFilePath() const { return m_FilePath; } // TODO: This could be in base class? For transient just return empty string

        const TextureSpecification& GetSpecification() const { return m_Specification; }
        const SamplerSettings& GetSamplerSettings() const { return m_Specification.SamplerSettings; }

        virtual bool Reload() override;

    private:
        void CreateTextureFromData(unsigned char* data, uint32_t width, uint32_t height);

        TextureSpecification m_Specification;
        nvrhi::TextureHandle m_TextureHandle;
        std::string m_FilePath;
    };
}

