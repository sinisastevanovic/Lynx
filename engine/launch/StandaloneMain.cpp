// Compiled into the standalone executable of every game by lynx_add_game.

#include <Lynx/Engine.h>
#include <Lynx/GameModule.h>

#include <iostream>

int main(int argc, char** argv)
{
    const auto project = Lynx::Project::LoadFromCommandLine(argc, argv);
    if (!project)
    {
        std::cerr << project.error() << std::endl;
        return 1;
    }

    Lynx::Engine engine;
    engine.Initialize(*project);

    Lynx::IGameModule* gameModule = LynxCreateGameModule();
    engine.SetGameModule(gameModule);

    const bool isPlaying = engine.PlayStartScene();
    if (isPlaying)
        engine.Run();

    engine.Shutdown();
    LynxDestroyGameModule(gameModule);
    return isPlaying ? 0 : 1;
}
