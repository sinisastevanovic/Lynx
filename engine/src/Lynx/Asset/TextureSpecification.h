#pragma once
#include <string>

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
        Linear = 0,
        Nearest,
        Anisotropic
    };

    enum class TextureFormat
    {
        None = 0,
        RGBA8,              // Standard color (0-255)
        RG16F,              // High precision 16-bit
        RG32F,              // High precision 32-bit    
        R32I,               // Integer (ID buffer for example)
        Depth32,            // Depth Buffer
        Depth24Stencil8,    // Depth Buffer + Stencil
    };

    struct SamplerSettings
    {
        TextureWrap WrapMode = TextureWrap::Repeat;
        TextureFilter FilterMode = TextureFilter::Linear;

        bool operator==(const SamplerSettings& other) const
        {
            return this->WrapMode == other.WrapMode && this->FilterMode == other.FilterMode;
        }
    };

    struct TextureSpecification
    {
        uint32_t Width = 1;
        uint32_t Height = 1;

        TextureFormat Format = TextureFormat::RGBA8;
        
        SamplerSettings SamplerSettings;
        
        bool GenerateMips = false;
        bool IsSRGB = true;

        std::string DebugName = "Texture";
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
            return h1 ^ (h2 << 1);
        }
    };
}
