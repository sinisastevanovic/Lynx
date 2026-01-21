#pragma once

#include <sol/sol.hpp>

#include "Lynx/Asset/AssetRef.h"
#include "Lynx/Asset/Script.h"

namespace Lynx
{
    struct ScriptInstance
    {
        AssetRef<Script> ScriptAsset;
        sol::table Self;
        
        bool IsValid() const { return ScriptAsset && Self.valid(); }
        bool operator==(const ScriptInstance& other) const { return ScriptAsset.Handle == other.ScriptAsset.Handle; }
    };
    
    struct LuaScriptComponent
    {
        std::vector<ScriptInstance> Scripts;

        LuaScriptComponent() = default;
        LuaScriptComponent(const LuaScriptComponent&) = default;
    };
}
