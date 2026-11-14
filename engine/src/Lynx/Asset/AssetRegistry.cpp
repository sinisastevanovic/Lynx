#include "AssetRegistry.h"

#include <set>
#include <nlohmann/json.hpp>

#include "MeshSpecification.h"
#include "TextureSpecification.h"


namespace Lynx
{
    AssetRegistry::AssetRegistry(const ContentPaths& contentPaths)
        : m_ContentPaths(contentPaths)
    {
    }

    void AssetRegistry::LoadRegistry()
    {
        m_AssetMetadata.clear();
        m_PathToHandle.clear();
        m_FileWatchers.clear();

        auto scanAndWatch = [this](const char* label, const std::filesystem::path& directory)
        {
            if (!std::filesystem::exists(directory))
            {
                LX_CORE_ERROR("AssetRegistry: {0} directory does not exist: {1}", label, directory.string());
                return;
            }

            LX_CORE_INFO("AssetRegistry: Scanning {0} at {1}", label, directory.string());
            ScanDirectory(directory);

            m_FileWatchers.push_back(std::make_unique<FileWatcher>(directory, [this](FileAction action, const std::filesystem::path& path,
                const std::filesystem::path& newPath)
            {
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                m_FileEvents.push_back({ action, ContentPaths::Normalize(path), newPath.empty() ? newPath : ContentPaths::Normalize(newPath) });
            }));
        };

        scanAndWatch("Engine Resources", m_ContentPaths.EngineResources());
        scanAndWatch("Project Assets", m_ContentPaths.ProjectAssets());
    }

    void AssetRegistry::UpdateMetadata(AssetHandle handle, std::shared_ptr<AssetSpecification> spec)
    {
        if (m_AssetMetadata.contains(handle))
        {
            m_AssetMetadata[handle].Specification = spec;
            WriteMetadata(m_AssetMetadata[handle]);
        }
    }

    void AssetRegistry::Update()
    {
        std::vector<FileEvent> eventsToProcess;
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
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
            if (event.Action == FileAction::Renamed && (event.NewPath.extension() == ".tmp" || event.NewPath.extension() == ".bak"))
                continue;
            
            if (event.Path.extension() == ".lxmeta")
                continue;

            if (event.Path == m_IgnoreFileChanges)
            {
                m_IgnoreFileChanges = std::filesystem::path();
                continue;
            }

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

    void AssetRegistry::ProcessFile(const std::filesystem::path& path, std::shared_ptr<AssetSpecification> specification)
    {
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
            if (specification)
            {
                metadata.Specification = specification;
            }
            else if (metadata.Type == AssetType::Texture)
            {
                metadata.Specification = std::make_shared<TextureSpecification>();
            }
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

        if (metadata.Specification)
        {
            nlohmann::json settings;
            metadata.Specification->Serialize(settings);
            j["Settings"] = settings;
        }

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

        metadata.FilePath = metadataPath.parent_path() / metadataPath.stem();

        metadata.Specification = AssetSpecification::CreateFromType(metadata.Type);
        uint32_t diskVersion = 0;

        if (metadata.Specification)
        {
            if (j.contains("Settings"))
            {
                metadata.Specification->Deserialize(j["Settings"]);
                diskVersion = j["Settings"].value("Version", 0);
            }

            if (diskVersion < metadata.Specification->GetCurrentVersion())
            {
                LX_CORE_WARN("Migrating {0} from v{1} to v{2}", metadataPath.string(), diskVersion, metadata.Specification->GetCurrentVersion());
                WriteMetadata(metadata);
            }
        }

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
        // TODO: Currently when we rename files, the asset itself doesnt update it's filepath!
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
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (m_AssetMetadata.contains(handle))
            return m_AssetMetadata.at(handle);
        return m_NullMetadata;
    }

    const AssetMetadata& AssetRegistry::Get(const std::filesystem::path& assetPath) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        auto it = m_PathToHandle.find(ContentPaths::Normalize(assetPath));
        if (it != m_PathToHandle.end())
            return Get(it->second);
        return m_NullMetadata;
    }

    bool AssetRegistry::Contains(AssetHandle handle) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_AssetMetadata.contains(handle);
    }

    bool AssetRegistry::Contains(const std::filesystem::path& assetPath) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        return m_PathToHandle.contains(ContentPaths::Normalize(assetPath));
    }

    AssetHandle AssetRegistry::ImportAsset(const std::filesystem::path& assetPath)
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (AssetUtils::IsAssetExtensionSupported(assetPath))
        {
            const std::filesystem::path normalizedPath = ContentPaths::Normalize(assetPath);
            ProcessFile(normalizedPath);
            return m_PathToHandle[normalizedPath];
        }
        return AssetHandle::Null();
    }

    AssetHandle AssetRegistry::ImportAssetFromTemp(const std::filesystem::path& tempPath, const std::filesystem::path& destinationPath, std::shared_ptr<AssetSpecification> specification)
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (AssetUtils::IsAssetExtensionSupported(tempPath))
        {
            // TODO: Tell filewatchers to ignore events
            const std::filesystem::path normalizedDestination = ContentPaths::Normalize(destinationPath);
            m_IgnoreFileChanges = normalizedDestination;
            std::filesystem::rename(tempPath, normalizedDestination);
            ProcessFile(normalizedDestination, specification);
            return m_PathToHandle[normalizedDestination];
        }
        return AssetHandle::Null();
    }
}
