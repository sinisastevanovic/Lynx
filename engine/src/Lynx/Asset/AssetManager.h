#pragma once
#include "Asset.h"
#include "Texture.h"
#include "StaticMesh.h"

#include <nvrhi/nvrhi.h> // TODO: Remove nvrhi from AssetManger...


namespace Lynx
{
    class LX_API AssetManager
    {
    public:
        AssetManager(nvrhi::DeviceHandle device);

        // Checks cache. If exists, returns it. If not, loads from disk, caches it, and returns it.
        std::shared_ptr<Texture> GetTexture(const std::string& filepath);
        std::shared_ptr<Texture> GetDefaultTexture();
        std::shared_ptr<Texture> GetWhiteTexture();
        std::shared_ptr<Texture> GetErrorTexture();

        // Checks cache. If exists, returns it. If not, loads from disk, caches it, and returns it.
        std::shared_ptr<StaticMesh> GetMesh(const std::string& filepath);

        // TODO: Temporary function until we can actually create meshes somehow
        std::shared_ptr<StaticMesh> GetDefaultCube();

        // Fast lookup. Returns nullptr if invalid handle.
        std::shared_ptr<Asset> GetAsset(AssetHandle handle);

        // Helper to check if a specific handle is valid and of a specific type
        template<typename T>
        std::shared_ptr<T> GetAsset(AssetHandle handle) {
            auto asset = GetAsset(handle);
            // Ideally check asset->GetType() here for safety
            return std::static_pointer_cast<T>(asset);
        }

        bool IsAssetLoaded(AssetHandle handle) const;
  
    private:
        // Helper to register an asset once loaded
        void AddAssetToCache(const std::string& filepath, std::shared_ptr<Asset> asset);
  
    private:
        nvrhi::DeviceHandle m_Device;
        
        // The Primary Storage (Owner)
        std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_Assets;

        // The Lookup Cache (Path -> UUID)
        // This ensures we don't load "wood.png" twice, even if we ask for it 100 times.
        std::unordered_map<std::string, AssetHandle> m_AssetPaths;
    };
}
