#pragma once

#include <cstdint>
#include <filesystem>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// What the caches on disk share: a hash that is the same in every build, and reading and writing
// a file as a flat run of numbers, length-prefixed strings and length-prefixed arrays.
namespace Lynx
{
    // FNV-1a. std::hash is not guaranteed to give the same value in another build.
    inline constexpr uint64_t HashSeed = 0xcbf29ce484222325ull;
    uint64_t HashBytes(uint64_t hash, const void* data, size_t size);

    // The hash of everything a file holds. Empty if the file cannot be read, which no stored hash equals.
    std::optional<uint64_t> HashFile(const std::filesystem::path& file);

    class BinaryWriter
    {
    public:
        explicit BinaryWriter(std::ostream& stream) : m_Stream(stream) {}

        template<typename Number>
            requires std::is_trivially_copyable_v<Number>
        void Write(const Number& value) { m_Stream.write(reinterpret_cast<const char*>(&value), sizeof(value)); }

        void Write(std::string_view text)
        {
            Write(static_cast<uint32_t>(text.size()));
            m_Stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        }

        template<typename Element>
            requires std::is_trivially_copyable_v<Element>
        void WriteArray(const std::vector<Element>& elements)
        {
            Write(static_cast<uint64_t>(elements.size()));
            m_Stream.write(reinterpret_cast<const char*>(elements.data()), static_cast<std::streamsize>(elements.size() * sizeof(Element)));
        }

    private:
        std::ostream& m_Stream;
    };

    // Every read reports failure, so a truncated or damaged file is treated as missing.
    class BinaryReader
    {
    public:
        explicit BinaryReader(std::istream& stream) : m_Stream(stream) {}

        template<typename Number>
            requires std::is_trivially_copyable_v<Number>
        bool Read(Number& value) { return static_cast<bool>(m_Stream.read(reinterpret_cast<char*>(&value), sizeof(value))); }

        bool Read(std::string& text)
        {
            uint32_t size = 0;
            if (!Read(size) || size > MaxStringSize)
                return false;
            text.resize(size);
            return static_cast<bool>(m_Stream.read(text.data(), size));
        }

        // The limit keeps a damaged length from asking for all the memory there is.
        template<typename Element>
            requires std::is_trivially_copyable_v<Element>
        bool ReadArray(std::vector<Element>& elements, uint64_t largestCount)
        {
            uint64_t count = 0;
            if (!Read(count) || count > largestCount)
                return false;
            elements.resize(static_cast<size_t>(count));
            return static_cast<bool>(m_Stream.read(reinterpret_cast<char*>(elements.data()), static_cast<std::streamsize>(count * sizeof(Element))));
        }

    private:
        static constexpr uint32_t MaxStringSize = 4096;
        std::istream& m_Stream;
    };
}
