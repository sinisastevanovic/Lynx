#pragma once
#include "AssetRegistry.h"
#include "AssetMetadata.h"
#include "Asset.h"
#include "Texture.h"
#include "StaticMesh.h"
#include <queue>

namespace Lynx
{
    enum class AssetLoadMode
    {
        Async,
        Blocking
    };
    
    class LX_API AssetManager
    {
    public:
        AssetManager(AssetRegistry* registy);

        // Returns the asset if loaded. If not loaded, uses Registry to find path, loads it, caches it, returns it.
        std::shared_ptr<Asset> GetAsset(AssetHandle handle, AssetLoadMode mode = AssetLoadMode::Async, std::function<void(AssetHandle)> onLoaded = nullptr);

        // Returns the asset if loaded. If not loaded, uses Registry to find path, loads it, caches it, returns it.
        template<typename T>
        std::shared_ptr<T> GetAsset(AssetHandle handle, AssetLoadMode mode = AssetLoadMode::Async, std::function<void(AssetHandle)> onLoaded = nullptr)
        {
            auto asset = GetAsset(handle, mode, onLoaded);
            if (asset)
                return std::static_pointer_cast<T>(asset);

            return GetErrorAsset<T>();
        }

        std::shared_ptr<Asset> GetAsset(const std::filesystem::path& path, AssetLoadMode mode = AssetLoadMode::Async, std::function<void(AssetHandle)> onLoaded = nullptr);

        template<typename T>
        std::shared_ptr<T> GetAsset(const std::filesystem::path& path, AssetLoadMode mode = AssetLoadMode::Async, std::function<void(AssetHandle)> onLoaded = nullptr)
        {
            auto asset = GetAsset(path, mode, onLoaded);
            if (asset)
                return std::static_pointer_cast<T>(asset);

            return GetErrorAsset<T>();
        }

        AssetHandle GetAssetHandle(const std::filesystem::path& path) const;

        bool IsAssetLoaded(AssetHandle handle) const;
        std::filesystem::path GetAssetPath(AssetHandle handle) const;
        std::string GetAssetName(AssetHandle handle) const;
        void AddRuntimeAsset(std::shared_ptr<Asset> asset);

        std::shared_ptr<Texture> GetDefaultTexture();
        std::shared_ptr<Texture> GetWhiteTexture();
        std::shared_ptr<Texture> GetErrorTexture();
        std::shared_ptr<StaticMesh> GetDefaultCube();

        void Update();
        void ReloadAsset(AssetHandle handle);
  
    private:
        std::shared_ptr<Asset> LoadAsset(const AssetMetadata& metadata, AssetLoadMode mode, std::function<void(AssetHandle)> onLoaded);
        std::shared_ptr<Asset> CreateAssetInstance(const AssetMetadata& metadata);
        void TrackAsset(std::shared_ptr<Asset> asset);

        template<typename T>
        std::shared_ptr<T> GetErrorAsset() { return nullptr; }

        void UnloadAsset(AssetHandle handle);
  
    private:
        AssetRegistry* m_AssetRegistry = nullptr;
        
        std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_LoadedAssets;
        std::vector<std::weak_ptr<Asset>> m_TrackedAssets;
        
        mutable std::mutex m_AssetsMutex;

        std::vector<std::function<void()>> m_MainThreadQueue;
        std::mutex m_QueueMutex;
    };

    template<>
    inline std::shared_ptr<Texture> AssetManager::GetErrorAsset<Texture>()
    {
        return GetErrorTexture();
    }
}
