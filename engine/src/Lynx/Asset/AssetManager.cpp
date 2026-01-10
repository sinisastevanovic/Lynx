#include "AssetManager.h"

#include <tiny_gltf.h>

#include "Asset.h"
#include "Font.h"
#include "Material.h"
#include "Script.h"
#include "Shader.h"
#include "Sprite.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    AssetManager::AssetManager(AssetRegistry* registy)
        : m_AssetRegistry(registy)
    {
        GetDefaultTexture();
        GetWhiteTexture();
        GetErrorTexture();
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle, AssetLoadMode mode, std::function<void(AssetHandle)> onLoaded)
    {
        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            if (m_LoadedAssets.contains(handle))
            {
                auto asset = m_LoadedAssets[handle];
                if (asset->IsLoaded() && onLoaded)
                    onLoaded(handle);
                return asset;
            }
        }
        
        if (!m_AssetRegistry->Contains(handle))
        {
            LX_CORE_ERROR("AssetManager: Trying to get asset that does not exist ({0})!", handle);
            return nullptr; // Asset does not exist!
        }

        const AssetMetadata& metadata = m_AssetRegistry->Get(handle);
        return LoadAsset(metadata, mode, onLoaded);
    }

    std::shared_ptr<Asset> AssetManager::LoadAsset(const AssetMetadata& metadata, AssetLoadMode mode, std::function<void(AssetHandle)> onLoaded)
    {
        std::shared_ptr<Asset> newAsset = CreateAssetInstance(metadata);
        if (!newAsset)
            return nullptr;

        newAsset->SetState(AssetState::Loading);

        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            m_LoadedAssets[metadata.Handle] = newAsset;
        }
        

        auto loadTask = [this, newAsset, metadata, onLoaded]()
        {
            if (newAsset->LoadSourceData())
            {
                std::lock_guard<std::mutex> lock(m_AssetsMutex);
                m_MainThreadQueue.push_back([this, newAsset, metadata, onLoaded]()
                {
                    if (newAsset->CreateRenderResources())
                    {
                        if (newAsset->GetType() == AssetType::Texture)
                        {
                            std::lock_guard<std::mutex> lock(m_AssetsMutex);
                            for (auto& [handle, asset] : m_LoadedAssets)
                            {
                                if (asset->GetType() == AssetType::Material && asset->DependsOn(newAsset->GetHandle()))
                                {
                                    asset->IncrementVersion();
                                }
                            }
                        }
                        newAsset->SetState(AssetState::Ready);
                        if (onLoaded) onLoaded(newAsset->GetHandle());
                    }
                    else
                    {
                        newAsset->SetState(AssetState::Error);
                        LX_CORE_ERROR("Failed to upload asset to GPU: {0}", metadata.FilePath.string());
                    }
                });
            }
            else
            {
                newAsset->SetState(AssetState::Error);
                LX_CORE_ERROR("Failed to load asset source: {0}", metadata.FilePath.string());
            }
        };

        if (mode == AssetLoadMode::Blocking)
        {
            loadTask();
            Update();
        }
        else
        {
            // TODO: Note: std::async w/ launch::async works too, but we don't need the future here
            // Ideally use a thread pool later.
            std::thread(loadTask).detach();
        }

        return newAsset;
    }

    std::shared_ptr<Asset> AssetManager::CreateAssetInstance(const AssetMetadata& metadata)
    {
        std::shared_ptr<Asset> newAsset = nullptr;
        switch (metadata.Type)
        {
            case AssetType::Texture:
                {
                    std::shared_ptr<TextureSpecification> texSpec;
                    if (metadata.Specification)
                        texSpec = std::static_pointer_cast<TextureSpecification>(metadata.Specification);
                    else
                        texSpec = std::make_shared<TextureSpecification>();
                    
                    newAsset = std::make_shared<Texture>(metadata.FilePath.string(), *texSpec);
                    break;
                }
            case AssetType::StaticMesh:
                newAsset = std::make_shared<StaticMesh>(metadata.FilePath.string());
                break;
            case AssetType::Script:
                newAsset = std::make_shared<Script>(metadata.FilePath.string());
                break;
            case AssetType::Material:
                newAsset = std::make_shared<Material>(metadata.FilePath.string());
                break;
            case AssetType::Shader:
                newAsset = std::make_shared<Shader>(metadata.FilePath.string());
                break;
            case AssetType::Scene:
                newAsset = std::make_shared<Scene>(metadata.FilePath.string());
                break;
            case AssetType::Sprite:
                newAsset = std::make_shared<Sprite>(metadata.FilePath.string());
                break;
            case AssetType::Font:
                newAsset = std::make_shared<Font>(metadata.FilePath.string());
                break;
            case AssetType::None:
            case AssetType::SkeletalMesh:
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
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            for (auto& [otherHandle, otherAsset] : m_LoadedAssets)
            {
                if (otherAsset->DependsOn(asset->GetHandle()))
                {
                    otherAsset->IncrementVersion();
                }
            }
            asset->IncrementVersion();
        }
    }

    void AssetManager::UnloadAsset(AssetHandle handle)
    {
        // TODO: Are there possible bugs with this?
        // I mean if another object holds a shared_ptr to an Asset and we unload it here, it still has a reference.
        // But the AssetManager doesn't know about that. So if we request this asset again, the AssetManager creates a new one instead...
        if (m_LoadedAssets.contains(handle))
        {
            LX_CORE_INFO("AssetManager: Unloading asset {0}", handle);
            m_LoadedAssets.erase(handle);
        }
    }

    void AssetManager::UnloadAllGameAssets()
    {
        std::lock_guard<std::mutex> lock(m_AssetsMutex);
        
        for (auto it = m_LoadedAssets.begin(); it != m_LoadedAssets.end();)
        {
            auto asset = it->second;
            bool isEngine = false;
            
            std::string path = asset->GetFilePath();
            if (path.find("engine/resources") != std::string::npos)
                isEngine = true;
            
            if (!isEngine)
            {
                it = m_LoadedAssets.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(const std::filesystem::path& path, AssetLoadMode mode, std::function<void(AssetHandle)> onLoaded)
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

    void AssetManager::AddRuntimeAsset(std::shared_ptr<Asset> asset)
    {
        if (!asset)
            return;

        if (asset->GetHandle() == AssetHandle::Null())
            asset->SetHandle(AssetHandle());

        asset->SetIsRuntime(true);
        if (asset->GetState() == AssetState::None)
            asset->SetState(AssetState::Ready);
        
        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            m_LoadedAssets[asset->GetHandle()] = asset;
        }
    }

    std::shared_ptr<Texture> AssetManager::GetDefaultTexture()
    {
        auto asset = GetAsset("engine/resources/T_DefaultChecker.png", AssetLoadMode::Blocking);
        return std::static_pointer_cast<Texture>(asset);
    }

    std::shared_ptr<Texture> AssetManager::GetWhiteTexture()
    {
        auto asset = GetAsset("engine/resources/T_White.png", AssetLoadMode::Blocking);
        return std::static_pointer_cast<Texture>(asset);
    }

    std::shared_ptr<Texture> AssetManager::GetErrorTexture()
    {
        auto asset = GetAsset("engine/resources/T_Error.png", AssetLoadMode::Blocking);
        return std::static_pointer_cast<Texture>(asset);
    }


    std::shared_ptr<StaticMesh> AssetManager::GetDefaultCube()
    {
        auto asset = GetAsset("engine/resources/SM_DefaultCube.gltf", AssetLoadMode::Blocking);
        return std::static_pointer_cast<StaticMesh>(asset);
    }

    std::shared_ptr<Font> AssetManager::GetDefaultFont()
    {
        auto asset = GetAsset("engine/resources/Fonts/OpenSans/OpenSans-Regular.lxfont", AssetLoadMode::Blocking);
        return std::static_pointer_cast<Font>(asset);
    }

    void AssetManager::Update()
    {
        m_AssetRegistry->Update();

        const auto& changedHandles = m_AssetRegistry->GetChangedAssets();
        for (AssetHandle handle : changedHandles)
        {
            // If not in asset registry anymore, it was removed.
            if (!m_AssetRegistry->Contains(handle))
            {
                if (IsAssetLoaded(handle))
                {
                    UnloadAsset(handle);
                }
            }
            else if (IsAssetLoaded(handle))
            {
                ReloadAsset(handle);
            }
        }

        std::vector<std::function<void()>> queueToRun;
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            queueToRun.swap(m_MainThreadQueue);
        }
        
        for (auto& func : queueToRun)
            func();
    }
}
