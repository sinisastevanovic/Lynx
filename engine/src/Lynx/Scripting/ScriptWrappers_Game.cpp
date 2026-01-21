#include "ScriptWrappers.h"

#include "Lynx/Engine.h"
#include "Lynx/Input.h"

namespace Lynx
{
    namespace ScriptWrappers
    {
        void RegisterGameTypes(sol::state& lua)
        {
            auto game = lua.create_named_table("Game");
            game.set_function("SetPaused", [](bool paused)
            {
                Engine::Get().SetPaused(paused);
            });
            game.set_function("IsPaused", []()
            {
                return Engine::Get().IsPaused();
            });
        }

        void RegisterInput(sol::state& lua)
        {
            lua.new_usertype<Input>("Input",
                "GetButton", &Input::GetButton,
                "GetAxis", &Input::GetAxis
            );
        }
    }
}
