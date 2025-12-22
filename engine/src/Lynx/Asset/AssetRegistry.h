#pragma once

#include "AssetMetadata.h"

namespace Lynx
{
    class LX_API AssetRegistry
    {
    public:
        AssetRegistry() = default;

        void LoadRegistry(const std::filesystem::path& projectAssetDir, const std::filesystem::path& engineResourceDir);

        const AssetMetadata& Get(AssetHandle handle) const;
        const AssetMetadata& Get(const std::filesystem::path& assetPath) const;

        bool Contains(AssetHandle handle) const;
        bool Contains(const std::filesystem::path& assetPath) const;

        AssetHandle ImportAsset(const std::filesystem::path& assetPath);

    private:
        void ScanDirectory(const std::filesystem::path& directory);
        void ProcessFile(const std::filesystem::path& path);
        void WriteMetadata(const AssetMetadata& metadata);
        AssetMetadata ReadMetadata(const std::filesystem::path& metadataPath);

    private:
        std::unordered_map<AssetHandle, AssetMetadata> m_AssetMetadata;
        std::map<std::filesystem::path, AssetHandle> m_PathToHandle;

        AssetMetadata m_NullMetadata;
    };
}


