#pragma once

#include "Lynx/Asset/AssetManager.h"
#include "Lynx/Engine.h"

namespace Lynx
{
    template<typename T>
    struct AssetRef
    {
        AssetHandle Handle = AssetHandle::Null();
        mutable std::shared_ptr<T> Cached = nullptr;
        
        AssetRef() = default;
        AssetRef(AssetHandle handle) : Handle(handle) {}
        AssetRef(std::shared_ptr<T> asset)
            : Handle(asset ? asset->GetHandle() : AssetHandle::Null()), Cached(asset)
        {}
        
        std::shared_ptr<T> Get() const
        {
            if (!Cached && Handle.IsValid())
            {
                Cached = Engine::Get().GetAssetManager().GetAsset<T>(Handle, AssetLoadMode::Blocking); // TODO: Support async in here too!
            }
            return Cached;
        }
        
        T* operator->() const
        {
            auto ptr = Get();
            return ptr ? ptr.get() : nullptr;
        }
        
        operator bool() const { return Handle.IsValid(); }
        bool operator==(const AssetRef<T>& other) const { return Handle == other.Handle; }
        bool operator!=(const AssetRef<T>& other) const { return Handle != other.Handle; }
    };
}