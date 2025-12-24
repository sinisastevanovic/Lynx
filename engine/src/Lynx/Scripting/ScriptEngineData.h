#pragma once

#include <sol/sol.hpp>

namespace Lynx
{
    class ScriptEngineUtils
    {
    public:
        static void RegisterBasicTypes(sol::state& Lua);
    };

}
