#include "AssetManager.h"

#include "Asset.h"

namespace Lynx
{
    AssetManager::AssetManager(nvrhi::DeviceHandle device, AssetRegistry* registy)
        : m_Device(device), m_AssetRegistry(registy)
    {
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle)
    {
        if (m_LoadedAssets.contains(handle))
            return m_LoadedAssets[handle];

        if (!m_AssetRegistry->Contains(handle))
        {
            LX_CORE_ERROR("AssetManager: Trying to get asset that does not exist ({0})!", handle);
            return nullptr; // Asset does not exist!
        }

        const AssetMetadata& metadata = m_AssetRegistry->Get(handle);

        std::shared_ptr<Asset> asset = LoadAsset(metadata);
        if (asset)
        {
            m_LoadedAssets[handle] = asset;
        }

        return asset;
    }

    std::shared_ptr<Asset> AssetManager::LoadAsset(const AssetMetadata& metadata)
    {
        // TODO: We would replace this with some kind of loaders/importers?
        std::shared_ptr<Asset> newAsset = nullptr;
        switch (metadata.Type)
        {
            case AssetType::Texture:
                newAsset = std::make_shared<Texture>(metadata.FilePath.string());
                break;
            case AssetType::StaticMesh:
                newAsset = std::make_shared<StaticMesh>(metadata.FilePath.string());
                break;
            case AssetType::None:
            case AssetType::SkeletalMesh:
            case AssetType::Material:
            case AssetType::Shader:
            case AssetType::Scene:
            default: LX_CORE_ERROR("AssetType not supported yet ({0})!", static_cast<int>(metadata.Type)); return nullptr;
        }

        if (newAsset)
            newAsset->SetHandle(metadata.Handle);

        return newAsset;
    }

    void AssetManager::ReloadAsset(AssetHandle handle)
    {
        LX_CORE_INFO("Reloading Asset: {0}", handle);
        std::shared_ptr<Asset> asset = m_LoadedAssets[handle];
        if (asset->Reload())
        {
            LX_CORE_INFO("Asset reloaded successfully");
        }
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            LX_CORE_ERROR("AssetManager: Trying to get asset that doesn't exist! ({0})", path.string());
            return nullptr;
        }
        AssetHandle handle = m_AssetRegistry->ImportAsset(path);
        return GetAsset(handle);
    }

    AssetHandle AssetManager::GetAssetHandle(const std::filesystem::path& path) const
    {
        if (!std::filesystem::exists(path))
            return AssetHandle::Null();
        if (m_AssetRegistry->Contains(path))
            return m_AssetRegistry->Get(path).Handle;
        return AssetHandle::Null();
    }

    bool AssetManager::IsAssetLoaded(AssetHandle handle) const
    {
        return m_LoadedAssets.contains(handle);
    }

    std::filesystem::path AssetManager::GetAssetPath(AssetHandle handle) const
    {
        if (m_AssetRegistry->Contains(handle))
            return m_AssetRegistry->Get(handle).FilePath;
        return {};
    }

    std::string AssetManager::GetAssetName(AssetHandle handle) const
    {
        if (m_AssetRegistry->Contains(handle))
        {
            return m_AssetRegistry->Get(handle).FilePath.filename().string();
        }
        return "Invalid!";
    }

    std::shared_ptr<Texture> AssetManager::GetDefaultTexture()
    {
        auto asset = GetAsset("engine/resources/T_DefaultChecker.png");
        return std::static_pointer_cast<Texture>(asset);
    }

    std::shared_ptr<Texture> AssetManager::GetWhiteTexture()
    {
        auto asset = GetAsset("engine/resources/T_White.png");
        return std::static_pointer_cast<Texture>(asset);
    }

    std::shared_ptr<Texture> AssetManager::GetErrorTexture()
    {
        auto asset = GetAsset("engine/resources/T_Error.png");
        return std::static_pointer_cast<Texture>(asset);
    }


    std::shared_ptr<StaticMesh> AssetManager::GetDefaultCube()
    {
        auto asset = GetAsset("engine/resources/SM_DefaultCube.gltf");
        return std::static_pointer_cast<StaticMesh>(asset);
    }

    void AssetManager::Update()
    {
        m_AssetRegistry->Update();

        const auto& changedHandles = m_AssetRegistry->GetChangedAssets();
        for (AssetHandle handle : changedHandles)
        {
            if (IsAssetLoaded(handle))
            {
                LX_CORE_INFO("Hot Reloading Asset: {0}", (uint64_t)handle);
                ReloadAsset(handle);
            }
        }
    }
}
