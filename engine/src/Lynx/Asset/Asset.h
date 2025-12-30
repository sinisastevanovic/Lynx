#pragma once
#include "AssetTypes.h"
#include "Lynx/UUID.h"

namespace Lynx
{
    enum class AssetState
    {
        None,
        Loading,
        Ready,
        Error
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

        // TODO: Move these to protected
        virtual bool Reload() { LX_CORE_ERROR("AssetType does not support hot reloading yet!"); return false; }

        virtual bool LoadSourceData() { return true; }
        virtual bool CreateRenderResources() { return true; }

        virtual bool DependsOn(AssetHandle handle) const { return false; }
        bool IsRuntime() const { return m_IsRuntime; }

        bool IsLoaded() const { return m_State == AssetState::Ready; }
        bool IsError() const { return m_State == AssetState::Error; }
        AssetState GetState() const { return m_State; }
        uint32_t GetVersion() const { return m_Version; }


    protected:
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

