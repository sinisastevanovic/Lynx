#pragma once

#include <sol/sol.hpp>

namespace Lynx
{
    struct LuaScriptComponent
    {
        std::string ScriptPath;
        sol::table Self;

        LuaScriptComponent() = default;
        LuaScriptComponent(const LuaScriptComponent&) = default;
        LuaScriptComponent(const std::string& path) : ScriptPath(path) {}
    };
}