#pragma once
#include "Lynx.h"

class GameConfig
{
public:
    static inline std::shared_ptr<Prefab> XPOrbPrefab = nullptr;
    
    static void LoadAssets()
    {
        auto& am = Engine::Get().GetAssetManager();
        
        {
            auto asset = am.GetProjectAsset<Prefab>("Prefabs/XPOrb.lxprefab");
            if (asset)
                XPOrbPrefab = asset;
        }
    }
    
    static void UnloadAssets()
    {
        XPOrbPrefab.reset();
    }
};
