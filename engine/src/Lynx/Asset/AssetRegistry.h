#pragma once

#include "AssetMetadata.h"
#include "Lynx/Utils/FileWatcher.h"

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

        const std::unordered_map<AssetHandle, AssetMetadata>& GetMetadata() const { return m_AssetMetadata; }

        const std::vector<AssetHandle>& GetChangedAssets() const { return m_ProcessedChangedAssets; }

        void Update();
        
    private:
        void ScanDirectory(const std::filesystem::path& directory);
        void ProcessFile(const std::filesystem::path& path);
        void WriteMetadata(const AssetMetadata& metadata);
        AssetMetadata ReadMetadata(const std::filesystem::path& metadataPath);

    private:
        std::unordered_map<AssetHandle, AssetMetadata> m_AssetMetadata;
        std::map<std::filesystem::path, AssetHandle> m_PathToHandle;

        AssetMetadata m_NullMetadata;

        // FileWatcher
        std::unique_ptr<FileWatcher> m_FileWatcher;
        std::vector<std::filesystem::path> m_ChangedFiles;
        std::vector<AssetHandle> m_ProcessedChangedAssets;
        std::mutex m_Mutex;
    };
}


