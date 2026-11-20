#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace Lynx
{
    struct ShaderProgram;

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

        // A whole shader program, stored together with the files it was compiled from. Loading returns it
        // only if every one of those files still has the content it had then, so that a change to an imported
        // module is noticed without running the compiler.
        std::optional<ShaderProgram> LoadProgram(const std::filesystem::path& sourceFile) const;
        void StoreProgram(const std::filesystem::path& sourceFile, const std::vector<std::filesystem::path>& dependencies, const ShaderProgram& program) const;

    private:
        std::filesystem::path ProgramPath(const std::filesystem::path& sourceFile) const;
        std::filesystem::path EntryPath(uint64_t key) const;

        std::filesystem::path m_Directory;
    };
}
