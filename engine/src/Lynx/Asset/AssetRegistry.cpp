#include "AssetRegistry.h"

#include <set>
#include <nlohmann/json.hpp>



namespace Lynx
{
    void AssetRegistry::LoadRegistry(const std::filesystem::path& projectAssetDir, const std::filesystem::path& engineResourceDir)
    {
        m_AssetMetadata.clear();
        m_PathToHandle.clear();

        if (std::filesystem::exists(engineResourceDir))
        {
            LX_CORE_INFO("AssetRegistry: Scanning Engine Resources at {0}", engineResourceDir.string());
            ScanDirectory(engineResourceDir);
        }
        
        LX_CORE_INFO("AssetRegistry: Scanning Project Assets at {0}", projectAssetDir.string());
        ScanDirectory(projectAssetDir);

        // TODO: We only need this in editor, right? 
        m_FileWatcher = std::make_unique<FileWatcher>(projectAssetDir, [this](FileAction action, const std::filesystem::path& path,
            const std::filesystem::path& newPath)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_FileEvents.push_back({action, path, newPath});
        });
    }
    
    void AssetRegistry::Update()
    {
        std::vector<FileEvent> eventsToProcess;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            eventsToProcess = std::move(m_FileEvents);
        }

        m_ProcessedChangedAssets.clear();

        std::vector<FileEvent> filteredEvents;
        std::set<std::filesystem::path> addedFiles;
        std::set<std::filesystem::path> modifiedFiles;
        std::set<std::filesystem::path> renamedFiles;
        for (const auto& event : eventsToProcess)
        {
            if (is_directory(event.Path))
                continue;
            // Ignore temp files
            if (event.Path.extension() == ".tmp" || event.Path.extension() == ".bak")
                continue;
            if (event.Action == FileAction::Renamed && (event.NewPath.extension() == "tmp" || event.NewPath.extension() == ".bak"))
                continue;
            
            if (event.Path.extension() == ".lxmeta")
                continue;

            switch (event.Action)
            {
                case FileAction::Added:
                {
                    //LX_CORE_INFO("FileAction: File added ({0})", event.Path.string());
                    filteredEvents.push_back(event);
                    addedFiles.insert(event.Path);
                    break;
                }
                case FileAction::Removed:
                {
                    // If we get a removed event, but the file exists right now, it was likely an atomic save (remove+rename)
                    // Just handle as a modification for now.
                    if (std::filesystem::exists(event.Path))
                    {
                        if (modifiedFiles.contains(event.Path))
                            continue;
                        filteredEvents.push_back({FileAction::Modified, event.Path});
                        modifiedFiles.insert(event.Path);
                    }
                    else
                    {
                        filteredEvents.push_back(event);
                    }
                    break;
                }
                case FileAction::Modified:
                {
                    if (addedFiles.contains(event.Path))
                        continue;

                    if (modifiedFiles.contains(event.Path))
                        continue;

                    if (Contains(event.Path))
                    {
                        filteredEvents.push_back(event);
                        modifiedFiles.insert(event.Path);
                    }
                    else
                    {
                        filteredEvents.push_back({ FileAction::Added, event.Path });
                        addedFiles.insert(event.Path);
                    }
                    
                    break;
                }
                case FileAction::Renamed:
                {
                    bool oldWasTmp = (event.Path.extension() == ".tmp");
                    if (oldWasTmp)
                    {
                        if (Contains(event.Path))
                        {
                            if (modifiedFiles.contains(event.Path))
                                continue;
                            filteredEvents.push_back({ FileAction::Modified, event.Path });
                            modifiedFiles.insert(event.Path);
                        }
                        else
                        {
                            if (addedFiles.contains(event.Path))
                                continue;
                            filteredEvents.push_back({ FileAction::Added, event.Path });
                            addedFiles.insert(event.Path);
                        }
                    }
                    else
                    {
                        if (renamedFiles.contains(event.Path))
                            continue;
                        filteredEvents.push_back(event);
                        renamedFiles.insert(event.Path);
                    }
                    break;
                }
            }
        }

        // TODO: Maybe add these to a queue and process later? 
        for (const auto& event : filteredEvents)
        {
            switch (event.Action)
            {
                case FileAction::Added:
                {
                    LX_CORE_TRACE("FileAction: File added ({0})", event.Path.string());
                    ImportAsset(event.Path);
                    break;
                }
                case FileAction::Removed:
                {
                    LX_CORE_TRACE("FileAction: File removed ({0})", event.Path.string());
                    OnAssetRemoved(event.Path);
                    break;
                }
                case FileAction::Modified:
                {
                    LX_CORE_TRACE("FileAction: File modified ({0})", event.Path.string());
                    m_ProcessedChangedAssets.push_back(m_PathToHandle[event.Path]);
                    break;
                }
                case FileAction::Renamed:
                {
                    LX_CORE_TRACE("FileAction: File renamed (old: {0}   new: {1})", event.Path.string(), event.NewPath.string());
                    OnAssetRenamed(event.Path, event.NewPath);
                    break;
                }
            }
        }
    }

    void AssetRegistry::ScanDirectory(const std::filesystem::path& directory)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        {
            if (entry.is_regular_file())
            {
                auto path = entry.path();
                std::string extension = path.extension().string();

                if (extension == ".lxmeta")
                    continue;

                if (AssetUtils::IsAssetExtensionSupported(path))
                {
                    ProcessFile(path);
                }
            }
        }
    }

    void AssetRegistry::ProcessFile(const std::filesystem::path& path)
    {
        // TODO: ensure the path is either inside the engines resources folder or games asset folder
        // Also, we probably shouldn't work with absolute paths? Maybe like unreal ("GAME/Meshes/..." and "ENGINE/Meshes/...")?
        std::filesystem::path metaPath = path.string() + ".lxmeta";

        AssetMetadata metadata;

        if (std::filesystem::exists(metaPath))
        {
            // Load existing metadata
            metadata = ReadMetadata(metaPath);
            AssetType type = AssetUtils::GetAssetTypeFromExtension(path);
            LX_ASSERT(type == metadata.Type, "Asset type mismatch in metadata and file extension!");
            metadata.FilePath = path.string(); // Update path in case it moved
        }
        else
        {
            // Create new metadata
            metadata.Handle = AssetHandle();
            metadata.FilePath = path;
            metadata.Type = AssetUtils::GetAssetTypeFromExtension(path);
            WriteMetadata(metadata);
        }

        m_AssetMetadata[metadata.Handle] = metadata;
        m_PathToHandle[metadata.FilePath] = metadata.Handle;
    }

    void AssetRegistry::WriteMetadata(const AssetMetadata& metadata)
    {
        nlohmann::json j;
        j["UUID"] = static_cast<uint64_t>(metadata.Handle);
        j["Type"] = metadata.Type;

        std::string metaPath = metadata.FilePath.string() + ".lxmeta";
        std::ofstream of(metaPath);
        of << j.dump(4);
    }

    AssetMetadata AssetRegistry::ReadMetadata(const std::filesystem::path& metadataPath)
    {
        std::ifstream i(metadataPath);
        nlohmann::json j;
        i >> j;

        AssetMetadata metadata;
        if (j.contains("UUID"))
            metadata.Handle = AssetHandle(j["UUID"]);
        if (j.contains("Type"))
            metadata.Type = static_cast<AssetType>(j["Type"]);

        return metadata;
    }

    void AssetRegistry::OnAssetRemoved(const std::filesystem::path& assetPath)
    {
        if (Contains(assetPath))
        {
            AssetHandle handle = m_PathToHandle[assetPath];
            m_AssetMetadata.erase(handle);
            m_PathToHandle.erase(assetPath);

            std::filesystem::path metaPath = assetPath.string() + ".lxmeta";
            if (std::filesystem::exists(metaPath))
            {
                std::filesystem::remove(metaPath);
            }
            
            m_ProcessedChangedAssets.push_back(handle);
        }
    }

    void AssetRegistry::OnAssetRenamed(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
    {
        if (!Contains(oldPath))
            return;

        AssetHandle handle = m_PathToHandle[oldPath];
        AssetMetadata& metadata = m_AssetMetadata[handle];

        m_PathToHandle.erase(oldPath);
        metadata.FilePath = newPath;
        m_PathToHandle[newPath] = handle;

        std::filesystem::path oldMetaPath = oldPath.string() + ".lxmeta";
        std::filesystem::path newMetaPath = newPath.string() + ".lxmeta";

        if (std::filesystem::exists(oldMetaPath) && !std::filesystem::exists(newMetaPath))
        {
            try
            {
                std::filesystem::rename(oldMetaPath, newMetaPath);
            }
            catch (const std::exception& e)
            {
                LX_CORE_ERROR("AssetRegistry: Failed to rename meta file: {0}", e.what());
            }
        }
    }

    const AssetMetadata& AssetRegistry::Get(AssetHandle handle) const
    {
        if (m_AssetMetadata.contains(handle))
            return m_AssetMetadata.at(handle);
        return m_NullMetadata;
    }

    const AssetMetadata& AssetRegistry::Get(const std::filesystem::path& assetPath) const
    {
        if (m_PathToHandle.contains(assetPath))
            return Get(m_PathToHandle.at(assetPath));
        return m_NullMetadata;
    }

    bool AssetRegistry::Contains(AssetHandle handle) const
    {
        return m_AssetMetadata.contains(handle);
    }

    bool AssetRegistry::Contains(const std::filesystem::path& assetPath) const
    {
        return m_PathToHandle.contains(assetPath);
    }

    AssetHandle AssetRegistry::ImportAsset(const std::filesystem::path& assetPath)
    {
        if (AssetUtils::IsAssetExtensionSupported(assetPath))
        {
            ProcessFile(assetPath);
            return m_PathToHandle[assetPath];
        }
        return AssetHandle::Null();
    }

 
}
