#pragma once
#include "AssetRegistry.h"
#include "AssetMetadata.h"
#include "Asset.h"
#include "Texture.h"
#include "StaticMesh.h"

#include <nvrhi/nvrhi.h> // TODO: Remove nvrhi from AssetManger...


namespace Lynx
{
    class LX_API AssetManager
    {
    public:
        AssetManager(nvrhi::DeviceHandle device, AssetRegistry* registy);

        // Returns the asset if loaded. If not loaded, uses Registry to find path, loads it, caches it, returns it.
        std::shared_ptr<Asset> GetAsset(AssetHandle handle);

        // Returns the asset if loaded. If not loaded, uses Registry to find path, loads it, caches it, returns it.
        template<typename T>
        std::shared_ptr<T> GetAsset(AssetHandle handle)
        {
            auto asset = GetAsset(handle);
            if (asset)
                return std::static_pointer_cast<T>(asset);

            return GetErrorAsset<T>();
        }

        std::shared_ptr<Asset> GetAsset(const std::filesystem::path& path);

        template<typename T>
        std::shared_ptr<T> GetAsset(const std::filesystem::path& path)
        {
            auto asset = GetAsset(path);
            if (asset)
                return std::static_pointer_cast<T>(asset);

            return GetErrorAsset<T>();
        }

        AssetHandle GetAssetHandle(const std::filesystem::path& path) const;

        bool IsAssetLoaded(AssetHandle handle) const;
        std::filesystem::path GetAssetPath(AssetHandle handle) const;

        std::shared_ptr<Texture> GetDefaultTexture();
        std::shared_ptr<Texture> GetWhiteTexture();
        std::shared_ptr<Texture> GetErrorTexture();
        std::shared_ptr<StaticMesh> GetDefaultCube();

        void Update();
  
    private:
        std::shared_ptr<Asset> LoadAsset(const AssetMetadata& metadata);

        template<typename T>
        std::shared_ptr<T> GetErrorAsset() { return nullptr; }

        void ReloadAsset(AssetHandle handle);
  
    private:
        nvrhi::DeviceHandle m_Device;
        AssetRegistry* m_AssetRegistry = nullptr;
        
        std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_LoadedAssets;
    };

    template<>
    inline std::shared_ptr<Texture> AssetManager::GetErrorAsset<Texture>()
    {
        return GetErrorTexture();
    }
}
