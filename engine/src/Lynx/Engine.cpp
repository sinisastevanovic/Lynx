#include "lxpch.h"
#include "Engine.h"

// TODO: We only include this because of keycodes. Remove when we have own keycodes.
#include <GLFW/glfw3.h>

#include "GameModule.h"
#include "Input.h"

#include "Log.h"
#include "Renderer/Renderer.h"
#include "Event/KeyEvent.h"
#include "Event/WindowEvent.h"


namespace Lynx
{
    void Engine::Initialize()
    {
        Log::Init();
        LX_CORE_INFO("Initializing...");

        m_Window = Window::Create();
        m_Window->SetEventCallback([this](Event& event){ this->OnEvent(event); });
        
        m_EditorCamera = EditorCamera(45.0f, (float)m_Window->GetWidth() / (float)m_Window->GetHeight(), 0.1f, 1000.0f);

        m_Renderer = std::make_unique<Renderer>(m_Window->GetNativeWindow());

        m_Window->SetVSync(true);
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

            m_EditorCamera.OnUpdate(0.016f); // Fake delta time

            m_Renderer->BeginScene(m_EditorCamera);
            if (gameModule)
            {
                gameModule->OnUpdate(0.016f); // Fake delta time
            }
            m_Renderer->EndScene();
        }

        if (gameModule)
        {
            gameModule->OnShutdown();
        }
    }

    void Engine::Shutdown()
    {
        LX_CORE_INFO("Shutting down...");
        m_Renderer.reset();
        Log::Shutdown();
    }

    void Engine::OnEvent(Event& e)
    {
        m_EditorCamera.OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& event)
        {
            if (event.GetKeyCode() == KeyCode::Escape)
            {
                LX_CORE_WARN("Escape key pressed via event, closing application.");
                m_IsRunning = false;
                return true; 
            }
            return false;
        });

        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& event)
        {
            m_EditorCamera.SetViewportSize(event.GetWidth(), event.GetHeight());
            m_Renderer->OnResize(event.GetWidth(), event.GetHeight());
            return false;
        });
    }
}
