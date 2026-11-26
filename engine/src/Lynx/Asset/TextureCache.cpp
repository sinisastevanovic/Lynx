#include "TextureCache.h"

#include "Lynx/Core/BinaryFile.h"
#include "Lynx/Log.h"

#include <fstream>

namespace Lynx
{
    namespace
    {
        // Bump this when the stored form changes. A change to cooking itself is part of the cooking key.
        constexpr uint32_t s_TextureVersion = 1;
        constexpr uint32_t s_TextureMagicNumber = 0x5443584c; // "LXCT"

        // More than any texture has. A damaged file that claims more is refused before memory is reserved for it.
        constexpr uint64_t s_LargestSizeCount = 32;
        constexpr uint64_t s_LargestByteCount = uint64_t(1) << 32;

        // What tells whether a file is still the one a texture was cooked from.
        struct FileStamp
        {
            uint64_t ByteCount = 0;
            int64_t WrittenAt = 0;

            bool operator==(const FileStamp&) const = default;
        };

        std::optional<FileStamp> StampOf(const std::filesystem::path& file)
        {
            std::error_code error;
            FileStamp stamp;
            stamp.ByteCount = std::filesystem::file_size(file, error);
            if (error)
                return std::nullopt;

            stamp.WrittenAt = std::filesystem::last_write_time(file, error).time_since_epoch().count();
            if (error)
                return std::nullopt;
            return stamp;
        }
    }

    TextureCache::TextureCache(const std::filesystem::path& directory)
        : m_Directory(directory)
    {
        std::error_code error;
        std::filesystem::create_directories(m_Directory, error);
        if (error)
            LX_CORE_WARN("TextureCache: Cannot create {0}, textures will be cooked on every start: {1}", m_Directory.string(), error.message());
    }

    std::optional<CookedTexture> TextureCache::Load(const std::filesystem::path& sourceFile, uint64_t cookingKey) const
    {
        const std::optional<FileStamp> stamp = StampOf(sourceFile);
        if (!stamp)
            return std::nullopt;

        std::ifstream file(EntryPath(sourceFile), std::ios::binary);
        if (!file)
            return std::nullopt;

        BinaryReader reader(file);
        uint32_t magicNumber = 0;
        uint32_t version = 0;
        FileStamp storedStamp;
        uint64_t storedKey = 0;
        if (!reader.Read(magicNumber) || !reader.Read(version) || magicNumber != s_TextureMagicNumber || version != s_TextureVersion)
            return std::nullopt;
        if (!reader.Read(storedStamp) || !reader.Read(storedKey) || storedStamp != *stamp || storedKey != cookingKey)
            return std::nullopt;

        CookedTexture texture;
        if (!reader.Read(texture.Format) || !reader.Read(texture.Shape)
            || !reader.ReadArray(texture.Sizes, s_LargestSizeCount) || texture.Sizes.empty()
            || !reader.ReadArray(texture.Pixels, s_LargestByteCount))
            return std::nullopt;

        // A size that reaches past the pixels would have the GPU read what is not there.
        for (const CookedTexture::Size& size : texture.Sizes)
        {
            if (size.Width == 0 || size.Height == 0 || size.Offset > texture.Pixels.size() || size.ByteCount > texture.Pixels.size() - size.Offset)
                return std::nullopt;
        }
        return texture;
    }

    void TextureCache::Store(const std::filesystem::path& sourceFile, uint64_t cookingKey, const CookedTexture& texture) const
    {
        const std::optional<FileStamp> stamp = StampOf(sourceFile);
        if (!stamp || texture.Sizes.empty())
            return;

        const std::filesystem::path entryPath = EntryPath(sourceFile);

        // Written under another name and renamed, so a reader never sees a half-written entry.
        std::filesystem::path temporaryPath = entryPath;
        temporaryPath += ".tmp";

        {
            std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
            BinaryWriter writer(file);
            writer.Write(s_TextureMagicNumber);
            writer.Write(s_TextureVersion);
            writer.Write(*stamp);
            writer.Write(cookingKey);
            writer.Write(texture.Format);
            writer.Write(texture.Shape);
            writer.WriteArray(texture.Sizes);
            writer.WriteArray(texture.Pixels);

            if (!file)
            {
                LX_CORE_WARN("TextureCache: Cannot write {0}", temporaryPath.string());
                return;
            }
        }

        std::error_code error;
        std::filesystem::rename(temporaryPath, entryPath, error);
        if (error)
            LX_CORE_WARN("TextureCache: Cannot store {0}: {1}", entryPath.string(), error.message());
    }

    std::filesystem::path TextureCache::EntryPath(const std::filesystem::path& sourceFile) const
    {
        // Named after the source file's location, so each texture has one entry that is replaced when it changes.
        const std::string location = sourceFile.lexically_normal().generic_string();
        const uint64_t key = HashBytes(HashSeed, location.data(), location.size());

        char fileName[32];
        std::snprintf(fileName, sizeof(fileName), "%016llx.lxct", static_cast<unsigned long long>(key));
        return m_Directory / fileName;
    }
}
