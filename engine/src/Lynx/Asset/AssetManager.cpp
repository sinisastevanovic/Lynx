#include "AssetManager.h"

#include <tiny_gltf.h>

#include "Asset.h"
#include "Font.h"
#include "Material.h"
#include "Prefab.h"
#include "Terrain.h"
#include "Script.h"
#include "Shader.h"
#include "Sprite.h"
#include "Lynx/Engine.h"
#include "Lynx/Core/WorkerPool.h"
#include "Lynx/Event/AssetEvents.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    namespace
    {
        unsigned int LoaderThreadCount()
        {
            // One core is left to the main thread. hardware_concurrency may report 0 when it does not know.
            const unsigned int cores = std::thread::hardware_concurrency();
            return std::clamp(cores > 1 ? cores - 1 : 1u, 1u, 4u);
        }
    }

    AssetManager::AssetManager(AssetRegistry* registry, const ContentPaths& contentPaths)
        : m_AssetRegistry(registry),
          m_ContentPaths(contentPaths),
          m_MainThreadId(std::this_thread::get_id()),
          m_Workers(std::make_unique<WorkerPool>(LoaderThreadCount()))
    {
    }

    AssetManager::~AssetManager()
    {
        // Joins the loader threads first. Their jobs use the members of this object.
        m_Workers.reset();
    }

    bool AssetManager::IsMainThread() const
    {
        return std::this_thread::get_id() == m_MainThreadId;
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle, AssetLoadMode mode)
    {
        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            const auto it = m_LoadedAssets.find(handle);
            if (it != m_LoadedAssets.end())
                return it->second;
        }

        if (!m_AssetRegistry->Contains(handle))
        {
            LX_CORE_ERROR("AssetManager: Trying to get asset that does not exist ({0})!", handle);
            return nullptr;
        }

        // A copy: the registry may change its entry while the load is under way.
        const AssetMetadata metadata = m_AssetRegistry->Get(handle);
        return LoadAsset(metadata, mode);
    }

    void AssetManager::SuggestTextureContent(AssetHandle texture, TextureContent content)
    {
        SuggestTextureSettings(texture, [content](TextureSpecification& settings) { settings.Content = content; });
    }

    void AssetManager::SuggestCutOutTexture(AssetHandle texture)
    {
        SuggestTextureSettings(texture, [](TextureSpecification& settings)
        {
            settings.Content = TextureContent::Color;
            settings.KeepsCoverage = true;
        });
    }

    void AssetManager::SuggestExactTexture(AssetHandle texture)
    {
        SuggestTextureSettings(texture, [](TextureSpecification& settings)
        {
            settings.Content = TextureContent::Data;
            settings.Compression = TextureCompression::None;
        });
    }

    void AssetManager::SuggestTextureSettings(AssetHandle texture, const std::function<void(TextureSpecification&)>& change)
    {
        if (!texture.IsValid() || !m_AssetRegistry->Contains(texture))
            return;

        const AssetMetadata metadata = m_AssetRegistry->Get(texture);
        if (metadata.Type != AssetType::Texture || !metadata.Specification)
            return;

        // A texture that is loaded, or being loaded, has its guess already or gets it any moment.
        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            if (m_LoadedAssets.contains(texture))
                return;
        }

        const TextureSpecification saved = *std::static_pointer_cast<TextureSpecification>(metadata.Specification);
        if (saved.Origin == TextureSettingsOrigin::Chosen)
            return;

        // Saying again what the settings say already is not worth writing a file for.
        TextureSpecification settings = saved;
        change(settings);
        settings.Origin = TextureSettingsOrigin::Suggested;
        if (settings != saved)
            m_AssetRegistry->UpdateMetadata(texture, std::make_shared<TextureSpecification>(settings));
    }

    bool AssetManager::HasPendingLoads() const
    {
        std::lock_guard<std::mutex> lock(m_AssetsMutex);
        for (const auto& [handle, asset] : m_LoadedAssets)
        {
            if (asset->GetState() == AssetState::Loading)
                return true;
        }
        return false;
    }

    std::shared_ptr<Asset> AssetManager::LoadAsset(const AssetMetadata& metadata, AssetLoadMode mode)
    {
        std::shared_ptr<Asset> asset = CreateAssetInstance(metadata);
        if (!asset)
            return nullptr;

        asset->SetState(AssetState::Loading);

        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            const auto [it, wasInserted] = m_LoadedAssets.try_emplace(metadata.Handle, asset);

            // Another caller started loading this asset after GetAsset looked for it. Theirs is the one to use.
            if (!wasInserted)
                return it->second;
        }

        if (mode == AssetLoadMode::Blocking && IsMainThread())
        {
            if (LoadSourceData(asset))
                FinishLoad(asset);
        }
        else if (mode == AssetLoadMode::Blocking)
        {
            if (LoadSourceData(asset))
                FinishLoadOnMainThread(asset);
        }
        else
        {
            m_Workers->Submit([this, asset]()
            {
                if (LoadSourceData(asset))
                    FinishLoadOnMainThread(asset);
            });
        }

        return asset;
    }

    bool AssetManager::LoadSourceData(const std::shared_ptr<Asset>& asset)
    {
        if (asset->LoadSourceData())
            return true;

        asset->SetState(AssetState::Error);
        LX_CORE_ERROR("Failed to load asset source: {0}", asset->GetFilePath());
        return false;
    }

    void AssetManager::FinishLoad(const std::shared_ptr<Asset>& asset)
    {
        if (!asset->CreateRenderResources())
        {
            asset->SetState(AssetState::Error);
            LX_CORE_ERROR("Failed to upload asset to GPU: {0}", asset->GetFilePath());
            return;
        }

        // A guess at what a texture holds is saved with the texture: it is then made once, shows in
        // the editor and stays as whoever knows better changes it.
        if (asset->GetType() == AssetType::Texture)
        {
            const Texture& texture = static_cast<const Texture&>(*asset);
            if (texture.HasGuessedSettings() && m_AssetRegistry->Contains(asset->GetHandle()))
                m_AssetRegistry->UpdateMetadata(asset->GetHandle(), std::make_shared<TextureSpecification>(texture.GetSpecification()));
        }

        // A material that was built while its texture was still loading has to pick the texture up now.
        if (asset->GetType() == AssetType::Texture)
        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            for (auto& [handle, other] : m_LoadedAssets)
            {
                if (other->GetType() == AssetType::Material && other->DependsOn(asset->GetHandle()))
                    other->IncrementVersion();
            }
        }

        asset->SetState(AssetState::Ready);
    }

    void AssetManager::FinishLoadOnMainThread(const std::shared_ptr<Asset>& asset)
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_MainThreadQueue.push_back([this, asset]() { FinishLoad(asset); });
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
            case AssetType::Prefab:
                newAsset = std::make_shared<Prefab>(metadata.FilePath.string());
                break;
            case AssetType::Terrain:
                newAsset = std::make_shared<Terrain>(metadata.FilePath.string());
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
        std::shared_ptr<Asset> asset;
        {
            std::lock_guard<std::mutex> lock(m_AssetsMutex);
            const auto it = m_LoadedAssets.find(handle);
            if (it == m_LoadedAssets.end())
                return;

            asset = it->second;
        }

        LX_CORE_INFO("Reloading Asset: {0}", handle);
        if (asset->Reload())
        {
            LX_CORE_INFO("Asset reloaded successfully");
            {
                std::lock_guard<std::mutex> lock(m_AssetsMutex);
                for (auto& [otherHandle, otherAsset] : m_LoadedAssets)
                {
                    if (otherAsset->DependsOn(asset->GetHandle()))
                    {
                        otherAsset->IncrementVersion();
                    }
                }
            }
            asset->IncrementVersion();
            AssetReloadedEvent e(handle);
            Engine::Get().OnEvent(e);
        }
    }

    void AssetManager::UnloadAsset(AssetHandle handle)
    {
        // TODO: Are there possible bugs with this?
        // I mean if another object holds a shared_ptr to an Asset and we unload it here, it still has a reference.
        // But the AssetManager doesn't know about that. So if we request this asset again, the AssetManager creates a new one instead...
        std::lock_guard<std::mutex> lock(m_AssetsMutex);
        if (m_LoadedAssets.erase(handle) > 0)
            LX_CORE_INFO("AssetManager: Unloading asset {0}", handle);
    }

    void AssetManager::UnloadAllGameAssets()
    {
        std::lock_guard<std::mutex> lock(m_AssetsMutex);
        
        for (auto it = m_LoadedAssets.begin(); it != m_LoadedAssets.end();)
        {
            if (m_ContentPaths.IsEngineResource(it->second->GetFilePath()))
                ++it;
            else
                it = m_LoadedAssets.erase(it);
        }
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(const std::filesystem::path& path, AssetLoadMode mode)
    {
        if (!std::filesystem::exists(path))
        {
            LX_CORE_ERROR("AssetManager: Trying to get asset that doesn't exist! ({0})", path.string());
            return nullptr;
        }
        AssetHandle handle = m_AssetRegistry->ImportAsset(path);
        return GetAsset(handle, mode);
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
        std::lock_guard<std::mutex> lock(m_AssetsMutex);
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

    void AssetManager::AddRuntimeAsset(std::shared_ptr<Asset> asset, AssetHandle handle)
    {
        if (!asset)
            return;

        asset->SetHandle(handle);
        AddRuntimeAsset(std::move(asset));
    }

    std::shared_ptr<Texture> AssetManager::GetDefaultTexture()
    {
        auto asset = GetAsset(m_ContentPaths.EngineResource("T_DefaultChecker.png"), AssetLoadMode::Blocking);
        return std::static_pointer_cast<Texture>(asset);
    }

    std::shared_ptr<Texture> AssetManager::GetWhiteTexture()
    {
        auto asset = GetAsset(m_ContentPaths.EngineResource("T_White.png"), AssetLoadMode::Blocking);
        return std::static_pointer_cast<Texture>(asset);
    }

    std::shared_ptr<Texture> AssetManager::GetErrorTexture()
    {
        auto asset = GetAsset(m_ContentPaths.EngineResource("T_Error.png"), AssetLoadMode::Blocking);
        return std::static_pointer_cast<Texture>(asset);
    }


    std::shared_ptr<StaticMesh> AssetManager::GetDefaultCube()
    {
        auto asset = GetAsset(m_ContentPaths.EngineResource("SM_DefaultCube.gltf"), AssetLoadMode::Blocking);
        return std::static_pointer_cast<StaticMesh>(asset);
    }

    std::shared_ptr<Font> AssetManager::GetDefaultFont()
    {
        auto asset = GetAsset(m_ContentPaths.EngineResource("Fonts/OpenSans/OpenSans-Regular.lxfont"), AssetLoadMode::Blocking);
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
