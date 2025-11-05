#include <Lynx/Engine.h>
#include <MyGameModule.h>
#include <iostream>

int main(int argc, char** argv)
{
    std::cout << "--- Starting Standalone Game ---" << std::endl;

    Lynx::Engine engine;
    
    // No DLLs! We create the game instance directly from its class.
    MyGame myGame;

    engine.Initialize();
    
    // We still register types, as systems might rely on the registry.
    myGame.RegisterComponents(&engine.ComponentRegistry);

    engine.Run(&myGame);

    engine.Shutdown();
    std::cout << "--- Game Finished ---" << std::endl;
    return 0;
}