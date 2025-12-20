#include "lxpch.h"
#include "Engine.h"
#include "GameModule.h"
#include "Input.h"

#include "Log.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Event/KeyEvent.h"
#include "Event/WindowEvent.h"


namespace Lynx
{
    Engine* Engine::s_Instance = nullptr;
    
    void Engine::Initialize()
    {
        s_Instance = this;
        Log::Init();
        LX_CORE_INFO("Initializing...");

        m_Window = Window::Create();
        m_Window->SetEventCallback([this](Event& event){ this->OnEvent(event); });
        
        m_EditorCamera = EditorCamera(45.0f, (float)m_Window->GetWidth() / (float)m_Window->GetHeight(), 0.1f, 1000.0f);
        
        m_Renderer = std::make_unique<Renderer>(m_Window->GetNativeWindow());
        m_Scene = std::make_shared<Scene>();

        m_AssetManager = std::make_unique<AssetManager>(m_Renderer->GetDeviceHandle());
        
        m_Window->SetVSync(true);

    }

    void Engine::Run(IGameModule* gameModule)
    {
        LX_CORE_INFO("Starting main loop...");

        if (gameModule)
        {
            gameModule->OnStart();
        }

        auto texture = m_AssetManager->GetTexture("assets/test.jpg");
        m_Renderer->SetTexture(texture);

        // This is a placeholder for the real game loop
        while (m_IsRunning)
        {
            if (m_Window->ShouldClose())
                m_IsRunning = false;
            
            m_Window->OnUpdate();

            m_EditorCamera.OnUpdate(0.016f); // Fake delta time

            m_Scene->OnUpdate(0.016f);

            if (gameModule)
            {
                gameModule->OnUpdate(0.016f); // Fake delta time
            }
            
            m_Renderer->BeginScene(m_EditorCamera);

            auto view = m_Scene->Reg().view<TransformComponent, MeshComponent>();
            for (auto entity : view)
            {
                auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
                m_Renderer->SubmitMesh(transform.GetTransform(), mesh.Color);
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
        m_AssetManager.reset();
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
