#pragma once
#include <sol/sol.hpp>

namespace Lynx
{
    namespace ScriptWrappers
    {
        void RegisterBasicTypes(sol::state& lua);
        void RegisterMath(sol::state& lua);
        void RegisterCoreTypes(sol::state& lua);
        // Extends the Entity type, so it has to run after RegisterCoreTypes.
        void RegisterComponentAccess(sol::state& lua);
        void RegisterUITypes(sol::state& lua);
        void RegisterGameTypes(sol::state& lua);
        void RegisterInput(sol::state& lua);
        void RegisterDebug(sol::state& lua);
    }
}