#pragma once

#include "TextureCooking.h"

namespace Lynx
{
    // The form a cooked texture is kept in on the GPU, by what it holds and how it is set to be compressed.
    //
    // Block compression keeps every four by four texels in a fixed number of bytes, which the GPU
    // reads without unpacking anything: a quarter of the memory for colours, and less to fetch for
    // every pixel that reads them.
    // - Colours and packed numbers become BC7: four channels, one byte a texel.
    // - A normal map becomes BC5: its two sideways channels, each as finely as a whole picture of one
    //   channel, one byte a texel. A shader works the third out, see normalFromMap in Material.slang.
    // - A picture of one channel becomes BC4: half a byte a texel.
    // - The pictures of a UI, and a texture set to no compression, stay as they are.
    TextureFormat CompressedFormatOf(const TextureSpecification& settings, TextureFormat cookedFormat);

    // The same texture with every size compressed into the format, which is one of the block formats.
    // The texture has to hold RGBA8 or R8 pixels, as cooking makes them.
    //
    // Encoding takes seconds for a large texture. It is done once, where the texture is cooked.
    CookedTexture CompressTexture(const CookedTexture& texture, TextureFormat format, const TextureSpecification& settings);
}
