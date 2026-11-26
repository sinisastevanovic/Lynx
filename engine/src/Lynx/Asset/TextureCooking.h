#pragma once

#include "TextureSpecification.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Lynx
{
    // A texture as it is after cooking: every size it is used in, ready to hand to the GPU.
    struct CookedTexture
    {
        // One size of the texture, and where its pixels lie in Pixels.
        struct Size
        {
            uint32_t Width = 0;
            uint32_t Height = 0;
            uint64_t Offset = 0;
            uint64_t ByteCount = 0;
        };

        // The form the pixels of every size are in.
        TextureFormat Format = TextureFormat::RGBA8;
        TextureShape Shape = TextureShape::Flat;

        // The sizes, the largest first, each half the one before. One if the texture has no smaller sizes.
        std::vector<Size> Sizes;
        std::vector<uint8_t> Pixels;

        uint32_t Width() const { return Sizes.empty() ? 0 : Sizes.front().Width; }
        uint32_t Height() const { return Sizes.empty() ? 0 : Sizes.front().Height; }
    };

    // Makes a texture ready for the GPU from the pixels of its file, as its settings say.
    // pixels has channelCount bytes a pixel, one or four. settings.Content has to be decided.
    //
    // The largest size is the file's pixels as they are, unless MaxSize makes it smaller.
    // Every smaller size is made from the one above it by what its content means:
    // - Colours are averaged as light. A file holds them as they go to a screen, where a middle value
    //   stands for a fifth of full brightness, so averaging the stored values makes every smaller
    //   size too dark at its edges and fine patterns.
    // - A normal map is averaged as the directions it holds, which are brought back to length one.
    // - Everything else is averaged as the numbers it is.
    // - With KeepsCoverage, the colours are averaged by how much each texel shows, so that what is
    //   cut away, usually black or white, does not seep into the edges. The alpha itself is averaged
    //   plainly: what keeps a cut-out from fading with distance is where its alpha is read from, see
    //   TextureSpecification::KeepsCoverage.
    CookedTexture CookTexture(std::span<const uint8_t> pixels, uint32_t width, uint32_t height, uint32_t channelCount, const TextureSpecification& settings);

    // A number that differs whenever cooking would give another result for the same file: for every
    // setting that cooking reads, and for every change to cooking itself.
    uint64_t TextureCookingKey(const TextureSpecification& settings);
}
