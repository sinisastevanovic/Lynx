#pragma once

#include "AssetMetadata.h"
#include "Lynx/Core/ContentPaths.h"
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
    
    // Knows every asset file of the engine and the project: its handle, its kind, its path and its
    // settings, which it keeps in a .lxmeta file next to the asset.
    //
    // Every function may be called from any thread: assets are loaded on worker threads, which look
    // their files up here while the main thread imports, renames and removes. So nothing hands out a
    // reference into the registry. What is returned is a copy, as things were at that moment, and
    // may be out of date a moment later: an asset that Contains said is there can be gone when Get
    // is called, which then returns metadata that is null.
    //
    // The settings of an asset are never changed in place. UpdateMetadata puts new ones in their
    // stead, so a copy of the metadata keeps the settings it was made with.
    class LX_API AssetRegistry
    {
    public:
        explicit AssetRegistry(const ContentPaths& contentPaths);

        // Scans the engine resources and the project assets, and starts watching both for changes.
        void LoadRegistry();

        AssetMetadata Get(AssetHandle handle) const;
        AssetMetadata Get(const std::filesystem::path& assetPath) const;

        bool Contains(AssetHandle handle) const;
        bool Contains(const std::filesystem::path& assetPath) const;

        AssetHandle ImportAsset(const std::filesystem::path& assetPath);
        AssetHandle ImportAssetFromTemp(const std::filesystem::path& tempPath, const std::filesystem::path& destinationPath, std::shared_ptr<AssetSpecification> specification = nullptr);

        // Every asset there is, for listing them.
        std::vector<AssetMetadata> GetAllMetadata() const;

        // The assets whose files changed on disk, as the last Update found.
        std::vector<AssetHandle> GetChangedAssets() const;

        void UpdateMetadata(AssetHandle handle, std::shared_ptr<AssetSpecification> spec);

        // Takes in what happened to the watched files since the last call. Called once a frame.
        void Update();
        
    private:
        void ScanDirectory(const std::filesystem::path& directory);
        void ProcessFile(const std::filesystem::path& path, std::shared_ptr<AssetSpecification> specification = nullptr);
        void WriteMetadata(const AssetMetadata& metadata);
        AssetMetadata ReadMetadata(const std::filesystem::path& metadataPath);

        void OnAssetRemoved(const std::filesystem::path& assetPath);
        void OnAssetRenamed(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);

    private:
        const ContentPaths& m_ContentPaths;

        std::unordered_map<AssetHandle, AssetMetadata> m_AssetMetadata;
        // Keyed by normalized absolute path, see ContentPaths::Normalize.
        std::map<std::filesystem::path, AssetHandle> m_PathToHandle;

        // FileWatcher
        std::vector<std::unique_ptr<FileWatcher>> m_FileWatchers;
        std::vector<FileEvent> m_FileEvents;
        std::vector<AssetHandle> m_ProcessedChangedAssets;

        std::filesystem::path m_IgnoreFileChanges;

        // Guards everything above. Recursive, because the public functions call each other.
        mutable std::recursive_mutex m_Mutex;
    };
}


