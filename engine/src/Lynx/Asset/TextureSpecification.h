#pragma once
#include <array>
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

        // One whole number from 0 to 65535, read by shaders as 0 to 1: heights. Added at the end,
        // because saved texture settings hold these values as numbers.
        R16,
    };

    // The shape of a texture. Only flat ones are loaded from files so far. The settings and the
    // cooked form carry the shape so that cubes can follow without changing either.
    enum class TextureShape
    {
        Flat = 0,

        // Six square pictures, read by a direction.
        Cube
    };

    // What the numbers in a texture mean. It decides how the texture is read, how its smaller sizes
    // are made and how it is compressed. Saved texture settings hold these values as numbers.
    enum class TextureContent
    {
        // Not looked at yet. The first load guesses from the picture, see GuessTextureSettings,
        // and saves the guess with the texture.
        Undecided = 0,

        // Colours painted for the screen (sRGB), which are lit: an albedo, an emissive picture.
        Color,

        // The way a surface faces, relative to the surface.
        NormalMap,

        // Numbers that are no colour, each channel on its own: roughness and metalness, masks.
        Data,

        // One number a pixel.
        Greyscale,

        // Colours that are shown as they are, and kept exact: icons, the pictures of a UI.
        UserInterface
    };

    inline constexpr std::array<const char*, 6> TextureContentNames = { "Undecided", "Colour", "Normal map", "Data (one number a channel)", "Greyscale", "User interface" };

    // Whether a texture is kept in fewer bytes than its pixels take.
    enum class TextureCompression
    {
        // As its content is best compressed.
        Automatic = 0,

        // Every pixel as it is in the file. For what compression would spoil.
        None
    };

    inline constexpr std::array<const char*, 2> TextureCompressionNames = { "Automatic", "None" };

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
        std::string DebugName = "Texture";

        TextureShape Shape = TextureShape::Flat;
        TextureContent Content = TextureContent::Undecided;
        TextureCompression Compression = TextureCompression::Automatic;

        // The alpha of this texture cuts shapes out, like leaves. Whether a pixel is cut away is then
        // decided from the texture's CutOutSizes finest sizes, however small it is on screen. Colours
        // still come from every size.
        //
        // A texture of twigs is mostly empty space with twigs in it. Each smaller size mixes more of
        // that emptiness into every texel, until the alpha lies below any cut-off: trees get barer
        // with distance and lose their shadows, which are drawn from the coarsest sizes of all.
        // Measured on the Trees benchmark against 64 samples a pixel read from the full size alone
        // (26 November 2026), as the distance in steps of 255: tested from the full size 3.3, from
        // the three finest 3.6, the five finest 12.8, every size 19.5. Remaking the alpha of the
        // smaller sizes does not help, because the GPU blends across texels and sizes after it:
        // Castano's scaling 19.5, Yuksel's alpha distribution 21.7, a test by chance (Wyman and
        // McGuire) 18.4. Raising the alpha with the size that is read, as the shaders used to, 7.2.
        bool KeepsCoverage = false;
        static constexpr uint32_t CutOutSizes = 3;

        // The texture is used no larger than this many pixels a side. Zero for as large as its file.
        uint32_t MaxSize = 0;

        TextureSpecification() = default;

        // The numbers of this texture are colours for the screen: a shader that reads them as such gets linear light.
        bool HoldsColor() const { return Content == TextureContent::Color || Content == TextureContent::UserInterface; }

        virtual uint32_t GetCurrentVersion() const override { return 3; }

        virtual void Serialize(nlohmann::json& json) const override
        {
            json["Version"] = GetCurrentVersion();
            json["TextureFormat"] = Format;
            json["WrapMode"] = SamplerSettings.WrapMode;
            json["FilterMode"] = SamplerSettings.FilterMode;
            json["GenerateMips"] = GenerateMips;
            json["UseAnisotropy"] = SamplerSettings.UseAnisotropy;
            json["Shape"] = Shape;
            json["Content"] = Content;
            json["Compression"] = Compression;
            json["KeepsCoverage"] = KeepsCoverage;
            json["MaxSize"] = MaxSize;
        }

        virtual void Deserialize(const nlohmann::json& json) override
        {
            Format = (TextureFormat)json["TextureFormat"];
            SamplerSettings.WrapMode = (TextureWrap)json["WrapMode"];
            SamplerSettings.FilterMode = (TextureFilter)json["FilterMode"];
            GenerateMips = json.value("GenerateMips", true);
            SamplerSettings.UseAnisotropy = json.value("UseAnisotropy", true);

            // Settings saved before version 3 have none of these: such a texture is guessed at when it is loaded.
            Shape = json.value("Shape", TextureShape::Flat);
            Content = json.value("Content", TextureContent::Undecided);
            Compression = json.value("Compression", TextureCompression::Automatic);
            KeepsCoverage = json.value("KeepsCoverage", false);
            MaxSize = json.value("MaxSize", 0u);
        }

        bool operator==(const TextureSpecification& other) const
        {
            return Format == other.Format &&
                    SamplerSettings == other.SamplerSettings &&
                    GenerateMips == other.GenerateMips &&
                    Shape == other.Shape && Content == other.Content && Compression == other.Compression &&
                    KeepsCoverage == other.KeepsCoverage && MaxSize == other.MaxSize;
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
