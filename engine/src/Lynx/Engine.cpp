#include "Engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "GameModule.h"
#include "Input.h"

#include "Log.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Components/Components.h"
#include "Event/KeyEvent.h"
#include "Event/WindowEvent.h"
#include "Scene/Components/PhysicsComponents.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

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

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(m_Window->GetNativeWindow(), true);
        m_Renderer->InitImGui();
        
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

        auto cubeMesh = m_AssetManager->GetDefaultCube();

        // This is a placeholder for the real game loop
        while (m_IsRunning)
        {
            float time = (float)glfwGetTime();
            float deltaTime = time - lastFrameTime;
            lastFrameTime = time;
            
            if (m_Window->ShouldClose())
                m_IsRunning = false;
            
            m_Window->OnUpdate();

            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (m_ImGuiCallback)
                m_ImGuiCallback();

            ImGui::Render();

            glm::mat4 cameraViewProj;
            glm::vec3 cameraPos;

            auto viewportSize = m_Renderer->GetViewportSize();

            bool foundCamera = false;
            {
                auto view = m_Scene->Reg().view<TransformComponent, CameraComponent>();
                for (auto entity : view)
                {
                    auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
                    if (camera.Primary)
                    {
                        // TODO: Maybe not do this every frame?
                        if (!camera.FixedAspectRatio)
                            camera.Camera.SetViewportSize(viewportSize.first, viewportSize.second);
                        
                        glm::mat4 viewMatrix = glm::inverse(transform.GetTransform());
                        cameraViewProj = camera.Camera.GetProjection() * viewMatrix;

                        foundCamera = true;
                        break;
                    }
                }
            }

            if (!foundCamera)
            {
                m_EditorCamera.SetViewportSize((float)viewportSize.first, (float)viewportSize.second);
                m_EditorCamera.OnUpdate(deltaTime);
                cameraViewProj = m_EditorCamera.GetViewProjection();
            }

            m_PhysicsSystem->Simulate(deltaTime);
            m_Scene->OnUpdate(deltaTime);
            if (gameModule)
            {
                gameModule->OnUpdate(deltaTime);
            }
            
            m_Renderer->BeginScene(cameraViewProj);
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
                    else
                    {
                        m_Renderer->SetTexture(nullptr);
                    }
                }
                
                m_Renderer->SubmitMesh(mesh, transform.GetTransform(), meshComp.Color);
            }

            if (true) // Render collider meshes
            {
                auto colliderView = m_Scene->Reg().view<TransformComponent, BoxColliderComponent>();
                m_Renderer->SetTexture(nullptr);

                for (auto entity : colliderView)
                {
                    auto [transform, collider] = colliderView.get<TransformComponent, BoxColliderComponent>(entity);
                    // We calculate a transform that matches the collider's bounds
                    // BoxCollider::HalfSize * 2 = Full Scale
                    glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), transform.Translation)
                        * glm::toMat4(transform.Rotation)
                        * glm::scale(glm::mat4(1.0f), collider.HalfSize * 2.0f);

                    m_Renderer->SubmitMesh(cubeMesh, colliderTransform, glm::vec4(0.0f, 1.0f, 0.0f, 0.5f));
                }
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
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
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
            m_Renderer->OnResize(event.GetWidth(), event.GetHeight());
            /*auto viewportSize = m_Renderer->GetViewportSize();
            m_EditorCamera.SetViewportSize(viewportSize.first, viewportSize.second);*/
            return false;
        });
    }
}
