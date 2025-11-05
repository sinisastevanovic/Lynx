#include "Engine.h"

// TODO: We only include this because of keycodes. Remove when we have own keycodes.
#include <GLFW/glfw3.h>

#include "GameModule.h"
#include "Input.h"

#include "Log.h"

namespace Lynx
{
    void Engine::Initialize()
    {
        Log::Init();
        LX_CORE_INFO("Initializing...");

        m_Window = Window::Create();
    }

    void Engine::Run(IGameModule* gameModule)
    {
        LX_CORE_INFO("Starting main loop...");

        if (gameModule)
        {
            gameModule->OnStart();
        }

        // This is a placeholder for the real game loop
        while (m_IsRunning)
        {
            if (m_Window->ShouldClose())
                m_IsRunning = false;
            
            m_Window->OnUpdate();
            // TODO: Poll events, start new frame, update ImGui, etc.

            if (Input::IsKeyPressed(GLFW_KEY_ESCAPE))
            {
                LX_CORE_WARN("Escape key pressed, closing application.");
                m_IsRunning = false;
            }

            if (gameModule)
            {
                gameModule->OnUpdate(0.016f); // Fake delta time
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
