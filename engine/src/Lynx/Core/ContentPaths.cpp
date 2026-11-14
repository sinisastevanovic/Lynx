#include "ContentPaths.h"

namespace Lynx
{
    namespace
    {
        const char* const s_ProjectAssetFolder = "assets";
        const char* const s_ProjectTempFolder = "temp";
    }

    ContentPaths::ContentPaths(const std::filesystem::path& engineResourceDir, const std::filesystem::path& projectDir)
        : m_EngineResources(Normalize(engineResourceDir)),
          m_ProjectRoot(Normalize(projectDir)),
          m_ProjectAssets(m_ProjectRoot / s_ProjectAssetFolder)
    {
    }

    std::filesystem::path ContentPaths::EngineResource(const std::filesystem::path& relativePath) const
    {
        return Normalize(m_EngineResources / relativePath);
    }

    std::filesystem::path ContentPaths::ProjectAsset(const std::filesystem::path& relativePath) const
    {
        return Normalize(m_ProjectAssets / relativePath);
    }

    bool ContentPaths::IsEngineResource(const std::filesystem::path& path) const
    {
        const std::filesystem::path relative = Normalize(path).lexically_relative(m_EngineResources);
        return !relative.empty() && *relative.begin() != "..";
    }

    std::filesystem::path ContentPaths::TempDirectory() const
    {
        std::filesystem::path directory = m_ProjectRoot / s_ProjectTempFolder;
        std::filesystem::create_directories(directory);
        return directory;
    }

    std::filesystem::path ContentPaths::Normalize(const std::filesystem::path& path)
    {
        return std::filesystem::absolute(path).lexically_normal();
    }

    std::filesystem::path ContentPaths::DefaultEngineResourceDir()
    {
        return LX_ENGINE_RESOURCE_DIR;
    }
}
