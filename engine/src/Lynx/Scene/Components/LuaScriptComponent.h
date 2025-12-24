#pragma once

#include <sol/sol.hpp>

namespace Lynx
{
    struct LuaScriptComponent
    {
        AssetHandle ScriptHandle = AssetHandle::Null();
        sol::table Self;

        LuaScriptComponent() = default;
        LuaScriptComponent(const LuaScriptComponent&) = default;
    };
}