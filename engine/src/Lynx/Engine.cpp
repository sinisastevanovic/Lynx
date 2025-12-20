#include "Engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

        m_PhysicsSystem = std::make_unique<PhysicsSystem>();
        
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

        float lastFrameTime = 0.0f;

        // This is a placeholder for the real game loop
        while (m_IsRunning)
        {
            float time = (float)glfwGetTime();
            float deltaTime = time - lastFrameTime;
            lastFrameTime = time;
            
            if (m_Window->ShouldClose())
                m_IsRunning = false;
            
            m_Window->OnUpdate();

            m_EditorCamera.OnUpdate(deltaTime);
            m_PhysicsSystem->Simulate(deltaTime);
            m_Scene->OnUpdate(deltaTime);
            if (gameModule)
            {
                gameModule->OnUpdate(deltaTime);
            }
            
            m_Renderer->BeginScene(m_EditorCamera);
            auto view = m_Scene->Reg().view<TransformComponent, MeshComponent>();
            for (auto entity : view)
            {
                auto [transform, meshComp] = view.get<TransformComponent, MeshComponent>(entity);

                auto mesh = m_AssetManager->GetAsset<StaticMesh>(meshComp.Mesh);
                if (mesh)
                {
                    if (mesh->GetTexture())
                    {
                        auto tex = m_AssetManager->GetAsset<Texture>(mesh->GetTexture());
                        m_Renderer->SetTexture(tex);
                    }
                }
                
                m_Renderer->SubmitMesh(mesh, transform.GetTransform(), meshComp.Color);
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
        m_PhysicsSystem.reset();
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
