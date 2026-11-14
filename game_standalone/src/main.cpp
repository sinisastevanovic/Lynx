#include <Lynx/Engine.h>
#include <MyGameModule.h>
#include <iostream>

int main(int argc, char** argv)
{
    std::cout << "--- Starting Standalone Game ---" << std::endl;

    Lynx::Engine engine;
    
    // No DLLs! We create the game instance directly from its class.
    MyGame myGame;

    // The project is the folder the game is started in, until projects get their own file.
    engine.Initialize(std::filesystem::current_path());

    engine.Run(&myGame);

    engine.Shutdown();
    std::cout << "--- Game Finished ---" << std::endl;
    return 0;
}