// Compiled into the module library of every game by lynx_add_game.
// Exposes the game to the editor, which loads the library at run time and looks these two symbols up by name.

#include <Lynx/GameModule.h>

#if defined(_WIN32)
    #define LX_MODULE_EXPORT __declspec(dllexport)
#else
    #define LX_MODULE_EXPORT __attribute__((visibility("default")))
#endif

extern "C"
{
    LX_MODULE_EXPORT Lynx::IGameModule* CreateGameModule()
    {
        return LynxCreateGameModule();
    }

    LX_MODULE_EXPORT void DestroyGameModule(Lynx::IGameModule* gameModule)
    {
        LynxDestroyGameModule(gameModule);
    }
}
