#include "TextureGuess.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace Lynx
{
    namespace
    {
        // About this many pixels are looked at, spread evenly over the picture.
        constexpr uint32_t PixelsLookedAt = 64 * 1024;

        // A normal map: nearly all of its pixels are arrows of about length one that point out of the
        // surface, and together they point straight out.
        constexpr float ShortestNormal = 0.85f;
        constexpr float LongestNormal = 1.15f;
        constexpr float ShareOfNormals = 0.95f;
        constexpr float MostSidewaysOnAverage = 0.15f;
        constexpr float LeastOutwardsOnAverage = 0.6f;

        // Alpha that cuts shapes out: some of the picture is cut away, and hardly any of it is in between.
        constexpr uint8_t AlphaOfNothing = 16;
        constexpr uint8_t AlphaOfEverything = 239;
        constexpr float LeastShareCutAway = 0.01f;
        constexpr float MostShareInBetween = 0.1f;

        // A normal map of rough ground, saved as a JPEG, passes that test only loosely: its arrows
        // lean every way and the file format bends them. With a name that says normal map, this much is enough.
        constexpr float ShortestNamedNormal = 0.7f;
        constexpr float LongestNamedNormal = 1.3f;
        constexpr float ShareOfNamedNormals = 0.7f;
        constexpr float ShareOutwardsOfNamedNormals = 0.9f;
        constexpr std::array<std::string_view, 6> NormalMapWords = { "n", "nor", "nrm", "norm", "normal", "normals" };

        // The words a file's name ends in, before a size like "2k", when it holds numbers and no
        // colour: as texture libraries and exporters name them.
        constexpr std::array<std::string_view, 17> DataWords = { "orm", "arm", "rma", "mr", "rm", "ao", "occlusion", "rough", "roughness",
            "metal", "metallic", "metalness", "metallicroughness", "mask", "masks", "height", "displacement" };

        // The words of a file's name, in small letters: "Rock_Face_03_nor_gl_2k.png" gives rock, face, 03, nor, gl, 2k.
        std::vector<std::string> WordsOf(const std::filesystem::path& file)
        {
            std::vector<std::string> words;
            std::string word;
            for (const char character : file.stem().string())
            {
                if (std::isalnum(static_cast<unsigned char>(character)))
                {
                    word += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                }
                else if (!word.empty())
                {
                    words.push_back(std::move(word));
                    word.clear();
                }
            }
            if (!word.empty())
                words.push_back(std::move(word));
            return words;
        }

        template<size_t Count>
        bool NamesOneOf(const std::vector<std::string>& words, const std::array<std::string_view, Count>& known)
        {
            // Only the last few words say what a picture holds. Before them stands what it shows.
            const size_t first = words.size() > 3 ? words.size() - 3 : 0;
            for (size_t index = first; index < words.size(); ++index)
            {
                if (std::find(known.begin(), known.end(), words[index]) != known.end())
                    return true;
            }
            return false;
        }
    }

    TextureGuess GuessTextureSettings(std::span<const uint8_t> pixels, uint32_t width, uint32_t height, uint32_t channelCount, const std::filesystem::path& file)
    {
        TextureGuess guess;
        if (channelCount == 1)
        {
            guess.Content = TextureContent::Greyscale;
            return guess;
        }

        const uint64_t pixelCount = static_cast<uint64_t>(width) * height;
        if (channelCount != 4 || pixelCount == 0 || pixels.size() < pixelCount * 4)
            return guess;

        const uint64_t step = std::max<uint64_t>(pixelCount / PixelsLookedAt, 1);
        uint64_t looked = 0;
        uint64_t normals = 0;
        uint64_t looseNormals = 0;
        uint64_t pointingOut = 0;
        uint64_t cutAway = 0;
        uint64_t inBetween = 0;
        double sideways[2] = { 0.0, 0.0 };
        double outwards = 0.0;

        // A step that divides the picture's width would look at a few columns only.
        for (uint64_t pixel = 0; pixel < pixelCount; pixel += step + (step > 1 && width % step == 0 ? 1 : 0))
        {
            const uint8_t* colour = pixels.data() + pixel * 4;
            const float x = colour[0] / 127.5f - 1.0f;
            const float y = colour[1] / 127.5f - 1.0f;
            const float z = colour[2] / 127.5f - 1.0f;
            const float length = std::sqrt(x * x + y * y + z * z);
            if (length >= ShortestNormal && length <= LongestNormal && z > 0.0f)
                ++normals;
            if (length >= ShortestNamedNormal && length <= LongestNamedNormal)
                ++looseNormals;
            if (z > 0.0f)
                ++pointingOut;
            sideways[0] += x;
            sideways[1] += y;
            outwards += z;

            if (colour[3] <= AlphaOfNothing)
                ++cutAway;
            else if (colour[3] < AlphaOfEverything)
                ++inBetween;
            ++looked;
        }

        const double count = static_cast<double>(looked);
        const bool looksLikeNormals = normals >= ShareOfNormals * count
            && std::abs(sideways[0] / count) <= MostSidewaysOnAverage && std::abs(sideways[1] / count) <= MostSidewaysOnAverage
            && outwards / count >= LeastOutwardsOnAverage;

        // The pixels are asked first, and a name never counts against them: a picture of bricks that
        // happens to be named "..._n" has colours that point into the surface, and is no normal map.
        const std::vector<std::string> words = WordsOf(file);
        const bool isNamedAndMayBeNormals = NamesOneOf(words, NormalMapWords)
            && looseNormals >= ShareOfNamedNormals * count && pointingOut >= ShareOutwardsOfNamedNormals * count;
        if (looksLikeNormals || isNamedAndMayBeNormals)
            guess.Content = TextureContent::NormalMap;
        else if (NamesOneOf(words, DataWords))
            guess.Content = TextureContent::Data;
        else
            guess.Content = TextureContent::Color;

        guess.KeepsCoverage = guess.Content == TextureContent::Color && cutAway >= LeastShareCutAway * count && inBetween <= MostShareInBetween * count;
        return guess;
    }
}
