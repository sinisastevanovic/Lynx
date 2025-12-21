#pragma once
#include "Lynx/Core.h"
#include "Asset.h"
#include <nvrhi/nvrhi.h>

namespace Lynx
{
    class LX_API Texture : public Asset
    {
    public:
        Texture(nvrhi::DeviceHandle device, const std::string& filepath);
        Texture(nvrhi::DeviceHandle device, std::vector<uint8_t> data, uint32_t width, uint32_t height, const std::string& debugName = "");
        virtual ~Texture() = default;

        static AssetType GetStaticType() { return AssetType::Texture; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        nvrhi::TextureHandle GetTextureHandle() const { return m_TextureHandle; }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        const std::string& GetFilePath() const { return m_FilePath; } // TODO: This could be in base class? For transient just return empty string

    private:
        nvrhi::TextureHandle m_TextureHandle;
        uint32_t m_Width;
        uint32_t m_Height;
        std::string m_FilePath;
    };
}

