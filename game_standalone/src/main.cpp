#include <Lynx/Engine.h>
#include <MyGameModule.h>
#include <iostream>

int main(int argc, char** argv)
{
    std::cout << "--- Starting Standalone Game ---" << std::endl;

    const auto project = Lynx::Project::LoadFromCommandLine(argc, argv);
    if (!project)
    {
        std::cerr << project.error() << std::endl;
        return 1;
    }

    Lynx::Engine engine;
    
    // No DLLs! We create the game instance directly from its class.
    MyGame myGame;

    engine.Initialize(*project);

    engine.Run(&myGame);

    engine.Shutdown();
    std::cout << "--- Game Finished ---" << std::endl;
    return 0;
}