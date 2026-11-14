#pragma once

#include "Lynx/Core.h"

#include <filesystem>

namespace Lynx
{
    // The absolute directories the engine reads content from and writes content to.
    // Built once at startup. Code that needs a file on disk asks this class,
    // so nothing depends on the process working directory.
    class LX_API ContentPaths
    {
    public:
        ContentPaths(const std::filesystem::path& engineResourceDir, const std::filesystem::path& projectRootDir, const std::filesystem::path& projectAssetDir);

        const std::filesystem::path& EngineResources() const { return m_EngineResources; }
        const std::filesystem::path& ProjectRoot() const { return m_ProjectRoot; }
        const std::filesystem::path& ProjectAssets() const { return m_ProjectAssets; }

        std::filesystem::path EngineResource(const std::filesystem::path& relativePath) const;
        std::filesystem::path ProjectAsset(const std::filesystem::path& relativePath) const;

        bool IsEngineResource(const std::filesystem::path& path) const;

        // Scratch space inside the project, created on first use. It shares a volume
        // with the project assets, so a finished file can be moved into place with a rename.
        std::filesystem::path TempDirectory() const;

        // Absolute, with "." and ".." resolved and separators unified,
        // so two spellings of the same file compare equal.
        static std::filesystem::path Normalize(const std::filesystem::path& path);

        // Where the resources of this engine build live.
        static std::filesystem::path DefaultEngineResourceDir();

    private:
        std::filesystem::path m_EngineResources;
        std::filesystem::path m_ProjectRoot;
        std::filesystem::path m_ProjectAssets;
    };
}
