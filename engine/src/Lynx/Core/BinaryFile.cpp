#include "BinaryFile.h"

#include <fstream>

namespace Lynx
{
    uint64_t HashBytes(uint64_t hash, const void* data, size_t size)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 0x100000001b3ull;
        }
        return hash;
    }

    std::optional<uint64_t> HashFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary | std::ios::ate);
        if (!stream)
            return std::nullopt;

        std::vector<char> content(static_cast<size_t>(stream.tellg()));
        stream.seekg(0);
        stream.read(content.data(), static_cast<std::streamsize>(content.size()));
        return HashBytes(HashSeed, content.data(), content.size());
    }
}
