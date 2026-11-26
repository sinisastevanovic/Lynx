#pragma once
#include "AssetTypes.h"
#include "Lynx/UUID.h"

#include <memory>

namespace Lynx
{
    struct AssetSpecification;
}

namespace Lynx
{
    enum class AssetState
    {
        None,
        Loading,
        Ready,
        Error
    };
    
    // What a reload reads from an asset's file, kept apart from the asset, which stays as it is and
    // in use until the reading is done. An asset whose file takes long to read makes one in
    // Asset::BeginReload, so that reloading it holds nothing up.
    class AssetReload
    {
    public:
        virtual ~AssetReload() = default;

        // Reads the file and works out everything that needs no GPU. Runs on a worker thread, and
        // touches nothing of the asset. False if the file could not be read.
        virtual bool Read() = 0;
    };

    class LX_API Asset
    {
    public:
        Asset() = default;
        Asset(const std::string& filePath) : m_FilePath(filePath) {}
        virtual ~Asset() = default;
        
        virtual AssetType GetType() const = 0;
        AssetHandle GetHandle() const { return m_Handle; }
        std::string GetFilePath() const { return m_FilePath; }

        virtual bool DependsOn(AssetHandle handle) const { return false; }
        bool IsRuntime() const { return m_IsRuntime; }

        bool IsLoaded() const { return m_State == AssetState::Ready; }
        bool IsError() const { return m_State == AssetState::Error; }
        AssetState GetState() const { return m_State; }
        uint32_t GetVersion() const { return m_Version; }


    protected:
        virtual bool Reload() { LX_CORE_ERROR("AssetType does not support hot reloading yet!"); return false; }

        // A reload in the background, in place of Reload. BeginReload notes what the reading needs of
        // the asset as it is now, and FinishReload takes over what was read: both on the main thread,
        // with AssetReload::Read on a worker thread in between. The asset is used as it was until
        // FinishReload. Null from BeginReload says the asset has no such reload and takes Reload.
        virtual std::unique_ptr<AssetReload> BeginReload() { return nullptr; }
        virtual bool FinishReload(AssetReload& reload) { return false; }

        virtual bool LoadSourceData() { return true; }
        virtual bool CreateRenderResources() { return true; }
        
        void SetHandle(AssetHandle handle) { m_Handle = handle; }
        void SetState(AssetState state) { m_State = state; }
        void IncrementVersion() { m_Version++; }
        void SetIsRuntime(bool runtime) { m_IsRuntime = runtime; }

        AssetHandle m_Handle;
        std::string m_FilePath;
        std::atomic<AssetState> m_State = AssetState::Ready;
        std::atomic<uint32_t> m_Version = 0;
        bool m_IsRuntime = false;

        friend class AssetManager;
    };
}

