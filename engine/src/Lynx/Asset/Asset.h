#pragma once
#include "AssetTypes.h"
#include "Lynx/UUID.h"

namespace Lynx
{
    /*struct AssetDragDropPayload
    {
        AssetHandle Handle;
        std::string Name;
    };*/
    
    class LX_API Asset
    {
    public:
        virtual ~Asset() = default;
        virtual AssetType GetType() const = 0;
        AssetHandle GetHandle() const { return m_Handle; }

        virtual bool Reload() { LX_CORE_ERROR("AssetType does not support hot reloading yet!"); return false; }

    protected:
        void SetHandle(AssetHandle handle) { m_Handle = handle; }
        AssetHandle m_Handle;

        friend class AssetManager;
    };
}

