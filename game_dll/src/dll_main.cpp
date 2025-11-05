#include <MyGameModule.h>

#if defined(_WIN32)
    #define API_EXPORT __declspec(dllexport)
#else
    #define API_EXPORT
#endif

extern "C" {
    API_EXPORT Lynx::IGameModule* CreateGameModule() {
        return new MyGame();
    }

    API_EXPORT void DestroyGameModule(Lynx::IGameModule* gameModule) {
        delete gameModule;
    }
}