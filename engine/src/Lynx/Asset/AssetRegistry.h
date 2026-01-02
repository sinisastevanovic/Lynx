#pragma once

#include "AssetMetadata.h"
#include "Lynx/Utils/FileWatcher.h"

namespace Lynx
{
    struct FileEvent
    {
        FileAction Action;
        std::filesystem::path Path;
        std::filesystem::path NewPath;

        bool operator==(const FileEvent& other) const
        {
            return Action == other.Action && Path == other.Path && NewPath == other.NewPath;
        }
    };
    
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

        void UpdateMetadata(AssetHandle handle, std::shared_ptr<AssetSpecification> spec);

        void Update();
        
    private:
        void ScanDirectory(const std::filesystem::path& directory);
        void ProcessFile(const std::filesystem::path& path);
        void WriteMetadata(const AssetMetadata& metadata);
        AssetMetadata ReadMetadata(const std::filesystem::path& metadataPath);

        void OnAssetRemoved(const std::filesystem::path& assetPath);
        void OnAssetRenamed(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);

    private:
        std::unordered_map<AssetHandle, AssetMetadata> m_AssetMetadata;
        std::map<std::filesystem::path, AssetHandle> m_PathToHandle;

        AssetMetadata m_NullMetadata;

        // FileWatcher
        std::vector<std::unique_ptr<FileWatcher>> m_FileWatchers;
        std::vector<FileEvent> m_FileEvents;
        std::vector<AssetHandle> m_ProcessedChangedAssets;

        mutable std::recursive_mutex m_Mutex;
    };
}


