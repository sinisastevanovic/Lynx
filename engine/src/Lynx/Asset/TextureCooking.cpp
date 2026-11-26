#include "TextureCooking.h"
#include "TextureCompression.h"

#include "Lynx/Core/BinaryFile.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Lynx
{
    namespace
    {
        // Bump this when cooking gives another result for the same file and settings.
        // 3: the alpha of a cut-out's smaller sizes is the plain average again, see TextureSpecification::KeepsCoverage.
        // 4: textures are block-compressed.
        constexpr uint32_t CookingVersion = 4;

        // What the pixels of a texture are averaged as.
        enum class Meaning
        {
            Numbers,
            Color,
            Directions
        };

        Meaning MeaningOf(const TextureSpecification& settings)
        {
            if (settings.Content == TextureContent::NormalMap)
                return Meaning::Directions;
            return settings.HoldsColor() ? Meaning::Color : Meaning::Numbers;
        }

        // From a colour as a file stores it to light, and back (the sRGB curve).
        float ToLight(float stored)
        {
            return stored <= 0.04045f ? stored / 12.92f : std::pow((stored + 0.055f) / 1.055f, 2.4f);
        }

        float FromLight(float light)
        {
            return light <= 0.0031308f ? light * 12.92f : 1.055f * std::pow(light, 1.0f / 2.4f) - 0.055f;
        }

        const std::array<float, 256>& LightOfByte()
        {
            static const std::array<float, 256> table = []
            {
                std::array<float, 256> values{};
                for (int byte = 0; byte < 256; ++byte)
                    values[byte] = ToLight(static_cast<float>(byte) / 255.0f);
                return values;
            }();
            return table;
        }

        uint8_t ToByte(float value)
        {
            return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        // One size of a texture as numbers to average: light for colours, directions for a normal map.
        struct Level
        {
            uint32_t Width = 0;
            uint32_t Height = 0;
            uint32_t ChannelCount = 4;
            std::vector<float> Values;

            const float* At(uint32_t x, uint32_t y) const { return Values.data() + (static_cast<size_t>(y) * Width + x) * ChannelCount; }
        };

        // The pixels of a file, read as the numbers that are averaged.
        struct Source
        {
            std::span<const uint8_t> Pixels;
            uint32_t Width = 0;
            uint32_t Height = 0;
            uint32_t ChannelCount = 4;
            Meaning ReadAs = Meaning::Numbers;

            void Read(uint32_t x, uint32_t y, float* values) const
            {
                const uint8_t* pixel = Pixels.data() + (static_cast<size_t>(y) * Width + x) * ChannelCount;
                if (ChannelCount == 1)
                {
                    values[0] = static_cast<float>(pixel[0]) / 255.0f;
                    return;
                }

                switch (ReadAs)
                {
                    case Meaning::Color:
                    {
                        const std::array<float, 256>& light = LightOfByte();
                        values[0] = light[pixel[0]];
                        values[1] = light[pixel[1]];
                        values[2] = light[pixel[2]];
                        break;
                    }
                    case Meaning::Directions:
                        values[0] = static_cast<float>(pixel[0]) / 127.5f - 1.0f;
                        values[1] = static_cast<float>(pixel[1]) / 127.5f - 1.0f;
                        values[2] = static_cast<float>(pixel[2]) / 127.5f - 1.0f;
                        break;
                    default:
                        values[0] = static_cast<float>(pixel[0]) / 255.0f;
                        values[1] = static_cast<float>(pixel[1]) / 255.0f;
                        values[2] = static_cast<float>(pixel[2]) / 255.0f;
                        break;
                }
                values[3] = static_cast<float>(pixel[3]) / 255.0f;
            }
        };

        struct LevelSource
        {
            const Level& From;
            uint32_t Width = 0;
            uint32_t Height = 0;

            void Read(uint32_t x, uint32_t y, float* values) const
            {
                const float* pixel = From.At(x, y);
                for (uint32_t channel = 0; channel < From.ChannelCount; ++channel)
                    values[channel] = pixel[channel];
            }
        };

        // Which pixels of the size above one pixel of a smaller size stands for along one axis, and by
        // how much each counts. Two, each by half, where the size above is even. Three where it is odd
        // and the smaller size is half of it rounded down: every pixel above then counts equally in all.
        struct Span
        {
            uint32_t First = 0;
            uint32_t Count = 2;
            std::array<float, 3> Weights{ 0.5f, 0.5f, 0.0f };
        };

        Span SpanOf(uint32_t index, uint32_t sizeAbove, uint32_t size)
        {
            Span span;
            if (sizeAbove == 1)
            {
                span.Count = 1;
                span.Weights = { 1.0f, 0.0f, 0.0f };
                return span;
            }

            span.First = index * 2;
            if (sizeAbove % 2 == 0)
                return span;

            const float all = static_cast<float>(sizeAbove);
            span.Count = 3;
            span.Weights = { static_cast<float>(size - index) / all, static_cast<float>(size) / all, static_cast<float>(index + 1) / all };
            return span;
        }

        // The next smaller size of what a source holds.
        template<typename From>
        Level HalfOf(const From& source, uint32_t channelCount, Meaning meaning, bool weighsByAlpha)
        {
            Level level;
            level.Width = std::max(source.Width / 2, 1u);
            level.Height = std::max(source.Height / 2, 1u);
            level.ChannelCount = channelCount;
            level.Values.resize(static_cast<size_t>(level.Width) * level.Height * channelCount);

            float* target = level.Values.data();
            for (uint32_t y = 0; y < level.Height; ++y)
            {
                const Span rows = SpanOf(y, source.Height, level.Height);
                for (uint32_t x = 0; x < level.Width; ++x, target += channelCount)
                {
                    const Span columns = SpanOf(x, source.Width, level.Width);

                    float sum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    float shown[3] = { 0.0f, 0.0f, 0.0f };
                    for (uint32_t row = 0; row < rows.Count; ++row)
                    {
                        for (uint32_t column = 0; column < columns.Count; ++column)
                        {
                            float values[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                            source.Read(columns.First + column, rows.First + row, values);

                            const float weight = rows.Weights[row] * columns.Weights[column];
                            for (uint32_t channel = 0; channel < channelCount; ++channel)
                                sum[channel] += values[channel] * weight;
                            if (weighsByAlpha)
                            {
                                for (uint32_t channel = 0; channel < 3; ++channel)
                                    shown[channel] += values[channel] * values[3] * weight;
                            }
                        }
                    }

                    // The colour of what shows. Where nothing shows, the plain average stands in.
                    if (weighsByAlpha && sum[3] > 1.0e-5f)
                    {
                        for (uint32_t channel = 0; channel < 3; ++channel)
                            sum[channel] = shown[channel] / sum[3];
                    }

                    if (meaning == Meaning::Directions && channelCount == 4)
                    {
                        const float length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
                        if (length > 1.0e-5f)
                        {
                            sum[0] /= length;
                            sum[1] /= length;
                            sum[2] /= length;
                        }
                    }

                    for (uint32_t channel = 0; channel < channelCount; ++channel)
                        target[channel] = sum[channel];
                }
            }
            return level;
        }

        // A level as the bytes a texture holds.
        void Append(std::vector<uint8_t>& pixels, const Level& level, Meaning meaning)
        {
            const size_t valueCount = level.Values.size();
            const size_t first = pixels.size();
            pixels.resize(first + valueCount);
            uint8_t* target = pixels.data() + first;

            if (level.ChannelCount == 1)
            {
                for (size_t index = 0; index < valueCount; ++index)
                    target[index] = ToByte(level.Values[index]);
                return;
            }

            for (size_t index = 0; index < valueCount; index += 4)
            {
                const float* values = level.Values.data() + index;
                for (int channel = 0; channel < 3; ++channel)
                {
                    switch (meaning)
                    {
                        case Meaning::Color: target[index + channel] = ToByte(FromLight(values[channel])); break;
                        case Meaning::Directions: target[index + channel] = ToByte(values[channel] * 0.5f + 0.5f); break;
                        default: target[index + channel] = ToByte(values[channel]); break;
                    }
                }
                target[index + 3] = ToByte(values[3]);
            }
        }

        uint32_t SizeCountFor(uint32_t width, uint32_t height)
        {
            uint32_t count = 1;
            for (uint32_t largest = std::max(width, height); largest > 1; largest /= 2)
                ++count;
            return count;
        }
    }

    // Every size of a texture as plain pixels, before any compression.
    static CookedTexture MakeSizes(std::span<const uint8_t> pixels, uint32_t width, uint32_t height, uint32_t channelCount, const TextureSpecification& settings)
    {
        CookedTexture cooked;
        cooked.Format = channelCount == 1 ? TextureFormat::R8 : TextureFormat::RGBA8;
        cooked.Shape = settings.Shape;
        if (width == 0 || height == 0 || (channelCount != 1 && channelCount != 4) || pixels.size() < static_cast<size_t>(width) * height * channelCount)
            return cooked;

        const Meaning meaning = MeaningOf(settings);
        const bool weighsByAlpha = settings.KeepsCoverage && channelCount == 4;

        // How many sizes at the top are left out because the texture may not be that large.
        uint32_t skipped = 0;
        if (settings.MaxSize > 0)
        {
            for (uint32_t largest = std::max(width, height); largest > settings.MaxSize; largest /= 2)
                ++skipped;
        }
        const uint32_t made = skipped + (settings.GenerateMips ? SizeCountFor(std::max(width >> skipped, 1u), std::max(height >> skipped, 1u)) : 1);

        const auto add = [&cooked](uint32_t sizeWidth, uint32_t sizeHeight, uint64_t byteCount)
        {
            cooked.Sizes.push_back({ sizeWidth, sizeHeight, cooked.Pixels.size() - byteCount, byteCount });
        };

        // The file's own pixels are the largest size, untouched.
        if (skipped == 0)
        {
            const size_t byteCount = static_cast<size_t>(width) * height * channelCount;
            cooked.Pixels.assign(pixels.begin(), pixels.begin() + byteCount);
            add(width, height, byteCount);
        }
        if (made == 1)
            return cooked;

        const Source source{ pixels, width, height, channelCount, meaning };
        Level level = HalfOf(source, channelCount, meaning, weighsByAlpha);

        for (uint32_t index = 1; index < made; ++index)
        {
            if (index >= skipped)
            {
                Append(cooked.Pixels, level, meaning);
                add(level.Width, level.Height, level.Values.size());
            }
            if (index + 1 < made)
                level = HalfOf(LevelSource{ level, level.Width, level.Height }, channelCount, meaning, weighsByAlpha);
        }
        return cooked;
    }

    CookedTexture CookTexture(std::span<const uint8_t> pixels, uint32_t width, uint32_t height, uint32_t channelCount, const TextureSpecification& settings)
    {
        // The sizes are made from plain pixels and compressed afterwards, each on its own: a
        // smaller size made from compressed texels would carry their flaws along and add its own.
        CookedTexture cooked = MakeSizes(pixels, width, height, channelCount, settings);
        if (cooked.Sizes.empty())
            return cooked;

        const TextureFormat format = CompressedFormatOf(settings, cooked.Format);
        return format == cooked.Format ? cooked : CompressTexture(cooked, format, settings);
    }

    uint64_t TextureCookingKey(const TextureSpecification& settings)
    {
        // Only what cooking reads: a change of filtering or wrapping needs no new cooking.
        struct Read
        {
            uint32_t Version;
            uint32_t Shape;
            uint32_t Content;
            uint32_t Compression;
            uint32_t KeepsCoverage;
            uint32_t MaxSize;
            uint32_t GenerateMips;
        };
        const Read read{ CookingVersion, static_cast<uint32_t>(settings.Shape), static_cast<uint32_t>(settings.Content), static_cast<uint32_t>(settings.Compression),
            settings.KeepsCoverage ? 1u : 0u, settings.MaxSize, settings.GenerateMips ? 1u : 0u };
        return HashBytes(HashSeed, &read, sizeof(read));
    }
}
