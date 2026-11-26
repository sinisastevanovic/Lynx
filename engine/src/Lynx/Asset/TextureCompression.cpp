#include "TextureCompression.h"

#include <bc7enc.h>
#include <rgbcx.h>

#include <algorithm>
#include <array>
#include <mutex>

namespace Lynx
{
    namespace
    {
        // A block is four by four texels, whatever format it is kept in.
        constexpr uint32_t BlockSide = 4;
        constexpr uint32_t TexelsPerBlock = BlockSide * BlockSide;

        uint32_t BytesPerBlock(TextureFormat format)
        {
            return format == TextureFormat::BC4 ? 8 : 16;
        }

        // The encoders fill tables of their own before their first block. Textures are cooked on
        // several threads at once, so that happens under a lock, once.
        void PrepareEncoders()
        {
            static std::once_flag prepared;
            std::call_once(prepared, []
            {
                bc7enc_compress_block_init();
                rgbcx::init();
            });
        }

        // The texels of one block of a size, row by row. A size is not always whole blocks wide: a
        // block that reaches past the edge repeats the last texel, which costs the encoder nothing
        // and never shows, because the GPU reads no texel past the edge.
        template<uint32_t ChannelCount>
        std::array<uint8_t, TexelsPerBlock * ChannelCount> BlockAt(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t blockX, uint32_t blockY)
        {
            std::array<uint8_t, TexelsPerBlock * ChannelCount> block;
            for (uint32_t row = 0; row < BlockSide; ++row)
            {
                const uint32_t y = std::min(blockY * BlockSide + row, height - 1);
                for (uint32_t column = 0; column < BlockSide; ++column)
                {
                    const uint32_t x = std::min(blockX * BlockSide + column, width - 1);
                    const uint8_t* texel = pixels + (static_cast<size_t>(y) * width + x) * ChannelCount;
                    std::copy_n(texel, ChannelCount, block.data() + (row * BlockSide + column) * ChannelCount);
                }
            }
            return block;
        }
    }

    TextureFormat CompressedFormatOf(const TextureSpecification& settings, TextureFormat cookedFormat)
    {
        if (settings.Compression == TextureCompression::None || settings.Content == TextureContent::UserInterface)
            return cookedFormat;
        if (cookedFormat == TextureFormat::R8)
            return TextureFormat::BC4;
        if (cookedFormat != TextureFormat::RGBA8)
            return cookedFormat;
        return settings.Content == TextureContent::NormalMap ? TextureFormat::BC5 : TextureFormat::BC7;
    }

    CookedTexture CompressTexture(const CookedTexture& texture, TextureFormat format, const TextureSpecification& settings)
    {
        PrepareEncoders();

        // Colours are compared as the eye sees them, where a step of green shows more than one of
        // blue. Numbers that are no colour count alike in every channel.
        bc7enc_compress_block_params colourParameters;
        bc7enc_compress_block_params_init(&colourParameters);
        if (settings.HoldsColor())
            bc7enc_compress_block_params_init_perceptual_weights(&colourParameters);
        else
            bc7enc_compress_block_params_init_linear_weights(&colourParameters);

        CookedTexture compressed;
        compressed.Format = format;
        compressed.Shape = texture.Shape;
        compressed.Sizes.reserve(texture.Sizes.size());

        const uint32_t blockBytes = BytesPerBlock(format);
        for (const CookedTexture::Size& size : texture.Sizes)
        {
            const uint32_t blocksAcross = (size.Width + BlockSide - 1) / BlockSide;
            const uint32_t blocksDown = (size.Height + BlockSide - 1) / BlockSide;
            const uint64_t byteCount = static_cast<uint64_t>(blocksAcross) * blocksDown * blockBytes;
            const uint64_t offset = compressed.Pixels.size();
            compressed.Pixels.resize(offset + byteCount);
            compressed.Sizes.push_back({ size.Width, size.Height, offset, byteCount });

            const uint8_t* pixels = texture.Pixels.data() + size.Offset;
            uint8_t* target = compressed.Pixels.data() + offset;
            for (uint32_t blockY = 0; blockY < blocksDown; ++blockY)
            {
                for (uint32_t blockX = 0; blockX < blocksAcross; ++blockX, target += blockBytes)
                {
                    switch (format)
                    {
                        case TextureFormat::BC4:
                        {
                            const auto block = BlockAt<1>(pixels, size.Width, size.Height, blockX, blockY);
                            rgbcx::encode_bc4(target, block.data(), 1);
                            break;
                        }
                        case TextureFormat::BC5:
                        {
                            // The first two channels of the four the texture has, each on its own.
                            const auto block = BlockAt<4>(pixels, size.Width, size.Height, blockX, blockY);
                            rgbcx::encode_bc5(target, block.data(), 0, 1, 4);
                            break;
                        }
                        default:
                        {
                            const auto block = BlockAt<4>(pixels, size.Width, size.Height, blockX, blockY);
                            bc7enc_compress_block(target, block.data(), &colourParameters);
                            break;
                        }
                    }
                }
            }
        }
        return compressed;
    }
}
