#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace Lynx
{
    // Keeps compiled SPIR-V on disk, so a shader is compiled only when its source changes.
    class ShaderCache
    {
    public:
        explicit ShaderCache(const std::filesystem::path& directory);

        // Identifies one compilation. The key changes with the source, the shader stage
        // and the compile settings, so a stale entry can never be returned for new input.
        static uint64_t MakeKey(std::string_view source, int stage);

        // Returns the cached SPIR-V, or an empty vector if there is none.
        std::vector<uint32_t> Load(uint64_t key) const;

        void Store(uint64_t key, const std::vector<uint32_t>& spirv) const;

    private:
        std::filesystem::path EntryPath(uint64_t key) const;

        std::filesystem::path m_Directory;
    };
}
