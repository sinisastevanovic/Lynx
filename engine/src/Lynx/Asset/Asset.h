#pragma once
#include "AssetTypes.h"
#include "Lynx/UUID.h"

namespace Lynx
{
    typedef UUID AssetHandle;
    
    class LX_API Asset
    {
    public:
        virtual ~Asset() = default;
        virtual AssetType GetType() const = 0;
        AssetHandle GetHandle() const { return m_Handle; }

    protected:
        AssetHandle m_Handle;
    };
}

