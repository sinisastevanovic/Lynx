#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace Lynx
{
    struct ShaderProgram;

    // Keeps compiled shader programs on disk, so a shader is compiled only when its source changes.
    class ShaderCache
    {
    public:
        explicit ShaderCache(const std::filesystem::path& directory);

        // A whole shader program, stored together with the files it was compiled from. Loading returns it
        // only if every one of those files still has the content it had then, so that a change to an imported
        // module is noticed without running the compiler.
        std::optional<ShaderProgram> LoadProgram(const std::filesystem::path& sourceFile) const;
        void StoreProgram(const std::filesystem::path& sourceFile, const std::vector<std::filesystem::path>& dependencies, const ShaderProgram& program) const;

    private:
        std::filesystem::path ProgramPath(const std::filesystem::path& sourceFile) const;

        std::filesystem::path m_Directory;
    };
}
