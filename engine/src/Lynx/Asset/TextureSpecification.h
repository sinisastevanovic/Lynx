#pragma once
#include <string>


#include "AssetSpecification.h"

namespace Lynx
{
    enum class TextureWrap
    {
        Repeat = 0,
        Clamp,
        Mirror
    };

    enum class TextureFilter
    {
        Bilinear = 0,
        Nearest,
        Trilinear
    };

    enum class TextureFormat
    {
        None = 0,
        RGBA8,              // Standard color (0-255)
        RG16F,              // High precision 16-bit
        RG32F,              // High precision 32-bit    
        R32I,               // Integer (ID buffer for example)
        R8,
        Depth32,            // Depth Buffer
        Depth24Stencil8,    // Depth Buffer + Stencil
    };

    struct SamplerSettings
    {
        TextureWrap WrapMode = TextureWrap::Repeat;
        TextureFilter FilterMode = TextureFilter::Trilinear;
        bool UseAnisotropy = true;

        bool operator==(const SamplerSettings& other) const
        {
            return this->WrapMode == other.WrapMode && this->FilterMode == other.FilterMode && this->UseAnisotropy == other.UseAnisotropy;
        }
    };

    struct TextureSpecification : public AssetSpecification
    {
        uint32_t Width = 1;
        uint32_t Height = 1;
        TextureFormat Format = TextureFormat::RGBA8;
        SamplerSettings SamplerSettings;
        bool GenerateMips = true;
        bool IsSRGB = false;
        std::string DebugName = "Texture";

        TextureSpecification() = default;

        virtual uint32_t GetCurrentVersion() const override { return 2; }

        virtual void Serialize(nlohmann::json& json) const override
        {
            json["Version"] = GetCurrentVersion();
            json["TextureFormat"] = Format;
            json["WrapMode"] = SamplerSettings.WrapMode;
            json["FilterMode"] = SamplerSettings.FilterMode;
            json["IsSRGB"] = IsSRGB;
            json["GenerateMips"] = GenerateMips;
            json["UseAnisotropy"] = SamplerSettings.UseAnisotropy;
        }

        virtual void Deserialize(const nlohmann::json& json) override
        {
            Format = (TextureFormat)json["TextureFormat"];
            SamplerSettings.WrapMode = (TextureWrap)json["WrapMode"];
            SamplerSettings.FilterMode = (TextureFilter)json["FilterMode"];
            IsSRGB = json.value("IsSRGB", false);
            GenerateMips = json.value("GenerateMips", true);
            SamplerSettings.UseAnisotropy = json.value("UseAnisotropy", true);
        }

        bool operator==(const TextureSpecification& other) const
        {
            return Format == other.Format &&
                    SamplerSettings == other.SamplerSettings &&
                    GenerateMips == other.GenerateMips &&
                    IsSRGB == other.IsSRGB;
        }
        bool operator!=(const TextureSpecification& other) const { return !(*this == other); }
    };
}

namespace std
{
    template<> struct hash<Lynx::SamplerSettings>
    {
        size_t operator()(const Lynx::SamplerSettings& sampler) const noexcept
        {
            size_t h1 = std::hash<int>{}((int)sampler.WrapMode);
            size_t h2 = std::hash<int>{}((int)sampler.FilterMode);
            size_t h3 = std::hash<bool>{}(sampler.UseAnisotropy);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}
