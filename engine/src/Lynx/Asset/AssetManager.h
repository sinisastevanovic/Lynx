#pragma once
#include "AssetRegistry.h"
#include "AssetMetadata.h"
#include "Asset.h"
#include "Texture.h"
#include "StaticMesh.h"
#include "Lynx/Core/ContentPaths.h"
#include <queue>

namespace Lynx
{
    class Font;
}

namespace Lynx
{
    class WorkerPool;

    enum class AssetLoadMode
    {
        // Returns at once. The asset is in the Loading state until a later AssetManager::Update finishes it.
        Async,

        // Returns the finished asset. Only the main thread can finish an asset, because that creates GPU resources:
        // called from another thread, the file is read before returning and the rest is left to the main thread.
        Blocking
    };
    
    class LX_API AssetManager
    {
    public:
        // Has to be created on the main thread, which it remembers as the thread that may create GPU resources.
        AssetManager(AssetRegistry* registry, const ContentPaths& contentPaths);
        ~AssetManager();

        // Returns the asset, and starts loading it if this is the first request for it.
        std::shared_ptr<Asset> GetAsset(AssetHandle handle, AssetLoadMode mode = AssetLoadMode::Async);

        template<typename T>
        std::shared_ptr<T> GetAsset(AssetHandle handle, AssetLoadMode mode = AssetLoadMode::Async)
        {
            auto asset = GetAsset(handle, mode);
            if (asset)
                return std::static_pointer_cast<T>(asset);

            return GetErrorAsset<T>();
        }

        std::shared_ptr<Asset> GetAsset(const std::filesystem::path& path, AssetLoadMode mode = AssetLoadMode::Async);

        template<typename T>
        std::shared_ptr<T> GetAsset(const std::filesystem::path& path, AssetLoadMode mode = AssetLoadMode::Async)
        {
            auto asset = GetAsset(path, mode);
            if (asset)
                return std::static_pointer_cast<T>(asset);

            return GetErrorAsset<T>();
        }

        // Loads an asset that ships with the engine. The path is relative to the engine resource directory.
        template<typename T>
        std::shared_ptr<T> GetEngineAsset(const std::filesystem::path& relativePath, AssetLoadMode mode = AssetLoadMode::Async)
        {
            return GetAsset<T>(m_ContentPaths.EngineResource(relativePath), mode);
        }

        // Loads an asset of the current project. The path is relative to the project asset directory.
        template<typename T>
        std::shared_ptr<T> GetProjectAsset(const std::filesystem::path& relativePath, AssetLoadMode mode = AssetLoadMode::Async)
        {
            return GetAsset<T>(m_ContentPaths.ProjectAsset(relativePath), mode);
        }

        AssetHandle GetAssetHandle(const std::filesystem::path& path) const;

        bool IsAssetLoaded(AssetHandle handle) const;

        // True while an asset is being read or waits for the main thread to finish it.
        bool HasPendingLoads() const;
        std::filesystem::path GetAssetPath(AssetHandle handle) const;
        std::string GetAssetName(AssetHandle handle) const;
        void AddRuntimeAsset(std::shared_ptr<Asset> asset);

        // The default assets are loaded on first use. Loading one needs the renderer to be initialized,
        // and the renderer needs this class for its shaders, so they cannot be loaded when this class is created.
        std::shared_ptr<Texture> GetDefaultTexture();
        std::shared_ptr<Texture> GetWhiteTexture();
        std::shared_ptr<Texture> GetErrorTexture();
        std::shared_ptr<StaticMesh> GetDefaultCube();
        std::shared_ptr<Font> GetDefaultFont();

        void Update();
        void ReloadAsset(AssetHandle handle);
        void UnloadAsset(AssetHandle handle);
        
        void UnloadAllGameAssets();
  
    private:
        std::shared_ptr<Asset> LoadAsset(const AssetMetadata& metadata, AssetLoadMode mode);
        std::shared_ptr<Asset> CreateAssetInstance(const AssetMetadata& metadata);

        // The two halves of a load. The first reads the file and may run on any thread.
        // The second creates the GPU resources and has to run on the main thread.
        bool LoadSourceData(const std::shared_ptr<Asset>& asset);
        void FinishLoad(const std::shared_ptr<Asset>& asset);
        void FinishLoadOnMainThread(const std::shared_ptr<Asset>& asset);

        bool IsMainThread() const;

        template<typename T>
        std::shared_ptr<T> GetErrorAsset() { return nullptr; }

  
    private:
        AssetRegistry* m_AssetRegistry = nullptr;
        const ContentPaths& m_ContentPaths;
        std::thread::id m_MainThreadId;

        // Guards m_LoadedAssets. Worker threads reach it too, through assets that load other assets.
        mutable std::mutex m_AssetsMutex;
        std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_LoadedAssets;

        // Work that worker threads leave for the main thread. Run by Update.
        std::mutex m_QueueMutex;
        std::vector<std::function<void()>> m_MainThreadQueue;

        // Declared last and reset first in the destructor: its jobs use everything above.
        std::unique_ptr<WorkerPool> m_Workers;
    };

    template<>
    inline std::shared_ptr<Texture> AssetManager::GetErrorAsset<Texture>()
    {
        return GetErrorTexture();
    }
}
