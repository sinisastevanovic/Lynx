#pragma once

#include <sol/sol.hpp>

#include "Lynx/Asset/AssetRef.h"
#include "Lynx/Asset/Script.h"

namespace Lynx
{
    struct LuaScriptComponent
    {
        AssetRef<Script> Script;
        sol::table Self;

        LuaScriptComponent() = default;
        LuaScriptComponent(const LuaScriptComponent&) = default;
    };
}
