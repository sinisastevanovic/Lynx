#include "lxpch.h"
#include "AssetManager.h"

#include "Asset.h"

namespace Lynx
{
    AssetManager::AssetManager(nvrhi::DeviceHandle device)
        : m_Device(device)
    {
    }

    std::shared_ptr<Texture> AssetManager::GetTexture(const std::string& filepath)
    {
        if (m_AssetPaths.contains(filepath))
        {
            AssetHandle handle = m_AssetPaths[filepath];
            return GetAsset<Texture>(handle);
        }

        std::shared_ptr<Texture> newTexture = std::make_shared<Texture>(m_Device, filepath);
        m_Assets[newTexture->GetHandle()] = newTexture;
        m_AssetPaths[filepath] = newTexture->GetHandle();

        LX_CORE_INFO("Loaded Texture: {0} (UUID: {1})", filepath, newTexture->GetHandle());
        return newTexture;
    }

    /*std::shared_ptr<Mesh> AssetManager::GetMesh(const std::string& filepath)
    {
    }*/

    std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle)
    {
        if (m_Assets.contains(handle))
        {
            return m_Assets[handle];
        }
        return nullptr;
    }

    bool AssetManager::IsAssetLoaded(AssetHandle handle) const
    {
        return m_Assets.contains(handle);
    }

    void AssetManager::AddAssetToCache(const std::string& filepath, std::shared_ptr<Asset> asset)
    {
    }
}
