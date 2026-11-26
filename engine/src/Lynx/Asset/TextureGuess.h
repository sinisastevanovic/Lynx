#pragma once

#include "TextureSpecification.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace Lynx
{
    // What a picture most likely holds, told from the picture itself, as far as a texture's settings
    // go: what its numbers mean, and whether its alpha cuts shapes out.
    struct TextureGuess
    {
        TextureContent Content = TextureContent::Color;
        bool KeepsCoverage = false;
    };

    // Guesses what a picture holds from its pixels and, where they cannot tell, from its file's name.
    // pixels has channelCount bytes a pixel, one or four.
    //
    // The pixels tell a normal map: its colours are arrows of length one that point out of the
    // surface, which no painted picture is. They tell a picture of one channel, and an alpha that
    // is nearly everywhere either none or all, which cuts shapes out. They cannot tell a colour from
    // numbers that only look like one, such as roughness and metalness packed into a picture: for
    // those the usual endings of file names are the hint.
    //
    // A guess is where a texture's settings start. Whoever knows better changes them.
    TextureGuess GuessTextureSettings(std::span<const uint8_t> pixels, uint32_t width, uint32_t height, uint32_t channelCount, const std::filesystem::path& file);
}
