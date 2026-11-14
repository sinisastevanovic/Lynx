#include "ShaderCache.h"

#include <fstream>

namespace Lynx
{
    namespace
    {
        // Bump this when the compile settings in ShaderUtils change, so entries built with the old settings are ignored.
        constexpr uint64_t s_CompileSettingsVersion = 1;

        constexpr uint32_t s_SpirvMagicNumber = 0x07230203;

        // FNV-1a. std::hash is not guaranteed to give the same value in another build, and the keys are file names.
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
    }

    ShaderCache::ShaderCache(const std::filesystem::path& directory)
        : m_Directory(directory)
    {
        std::error_code error;
        std::filesystem::create_directories(m_Directory, error);
        if (error)
            LX_CORE_WARN("ShaderCache: Cannot create {0}, shaders will be compiled on every start: {1}", m_Directory.string(), error.message());
    }

    uint64_t ShaderCache::MakeKey(std::string_view source, int stage)
    {
        uint64_t hash = 0xcbf29ce484222325ull;
        hash = HashBytes(hash, &s_CompileSettingsVersion, sizeof(s_CompileSettingsVersion));
        hash = HashBytes(hash, &stage, sizeof(stage));
        hash = HashBytes(hash, source.data(), source.size());
        return hash;
    }

    std::vector<uint32_t> ShaderCache::Load(uint64_t key) const
    {
        std::ifstream file(EntryPath(key), std::ios::binary | std::ios::ate);
        if (!file)
            return {};

        const std::streamsize byteCount = file.tellg();
        if (byteCount <= 0 || byteCount % sizeof(uint32_t) != 0)
            return {};

        std::vector<uint32_t> spirv(static_cast<size_t>(byteCount) / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(spirv.data()), byteCount);

        // A truncated or foreign file is treated as a miss, and the shader is compiled again.
        if (!file || spirv.front() != s_SpirvMagicNumber)
            return {};

        return spirv;
    }

    void ShaderCache::Store(uint64_t key, const std::vector<uint32_t>& spirv) const
    {
        const std::filesystem::path entryPath = EntryPath(key);

        // Written under another name and renamed, so a reader never sees a half-written entry.
        std::filesystem::path temporaryPath = entryPath;
        temporaryPath += ".tmp";

        {
            std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(spirv.data()), static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
            if (!file)
            {
                LX_CORE_WARN("ShaderCache: Cannot write {0}", temporaryPath.string());
                return;
            }
        }

        std::error_code error;
        std::filesystem::rename(temporaryPath, entryPath, error);
        if (error)
            LX_CORE_WARN("ShaderCache: Cannot store {0}: {1}", entryPath.string(), error.message());
    }

    std::filesystem::path ShaderCache::EntryPath(uint64_t key) const
    {
        char fileName[32];
        std::snprintf(fileName, sizeof(fileName), "%016llx.spv", static_cast<unsigned long long>(key));
        return m_Directory / fileName;
    }
}
