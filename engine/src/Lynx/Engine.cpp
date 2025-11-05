#include "Engine.h"
#include "GameModule.h"
#include <iostream>

#include "Log.h"

namespace Lynx
{
    void Engine::Initialize()
    {
        std::cout << "[Engine] Initializing..." << std::endl;
        Log::Init();
    }

    void Engine::Run(IGameModule* gameModule)
    {
        LX_CORE_INFO("Starting main loop...");

        if (gameModule)
        {
            gameModule->OnStart();
        }

        // This is a placeholder for the real game loop
        bool isRunning = true;
        while (isRunning)
        {
            // TODO: Poll events, start new frame, update ImGui, etc.

            if (gameModule)
            {
                gameModule->OnUpdate(0.016f); // Fake delta time
            }

            // A real loop would check for a window close event
            // For this example, we'll just break after a few prints
            static int frameCount = 0;
            if (++frameCount > 5)
            {
                isRunning = false;
            }
        }

        if (gameModule)
        {
            gameModule->OnShutdown();
        }
    }

    void Engine::Shutdown()
    {
        LX_CORE_INFO("Shutting down...");
        Log::Shutdown();
    }
}
