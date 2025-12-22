#include "AssetRegistry.h"

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
        ProcessFile(assetPath);
        return m_PathToHandle[assetPath];
    }

    

    
}
