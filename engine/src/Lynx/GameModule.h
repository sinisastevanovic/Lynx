#pragma once

#include "Lynx/Core.h"

namespace Lynx
{
    class GameTypeRegistry;

    class LX_API IGameModule
    {
    public:
        virtual ~IGameModule() = default;

        virtual void RegisterScripts(GameTypeRegistry* registry) = 0;
        virtual void RegisterComponents(GameTypeRegistry* registry) = 0;
        virtual void OnStart() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnShutdown() = 0;
    };

    extern "C" {
        typedef IGameModule* (*CreateGameFunc)();
        typedef void (*DestroyGameFunc)(IGameModule*);
    }
}

// Defined by the game through LX_GAME_MODULE.
// The launchers in engine/launch call them to create and destroy the game.
Lynx::IGameModule* LynxCreateGameModule();
void LynxDestroyGameModule(Lynx::IGameModule* gameModule);

// Names the class that implements the game. Put it in exactly one source file of the game:
//
//     LX_GAME_MODULE(MyGame)
#define LX_GAME_MODULE(GameModuleClass) \
    Lynx::IGameModule* LynxCreateGameModule() { return new GameModuleClass(); } \
    void LynxDestroyGameModule(Lynx::IGameModule* gameModule) { delete gameModule; }
