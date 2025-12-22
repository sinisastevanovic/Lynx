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
#include <nlohmann/json.hpp>

namespace Lynx
{
    Engine* Engine::s_Instance = nullptr;
    
    void Engine::Initialize()
    {
        s_Instance = this;
        Log::Init();
        LX_CORE_INFO("Initializing...");

        RegisterComponents();

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

            if (!foundCamera || m_SceneState == SceneState::Edit)
            {
                m_EditorCamera.SetViewportSize((float)viewportSize.first, (float)viewportSize.second);
                m_EditorCamera.OnUpdate(deltaTime);
                cameraViewProj = m_EditorCamera.GetViewProjection();
            }

            if (m_SceneState == SceneState::Play)
            {
                m_PhysicsSystem->Simulate(deltaTime);
                m_Scene->OnUpdateRuntime(deltaTime);
                if (gameModule)
                {
                    gameModule->OnUpdate(deltaTime);
                }
            }
            else
            {
                m_Scene->OnUpdateEditor(deltaTime);
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

            if (true && m_SceneState == SceneState::Edit) // Render collider meshes
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

    void Engine::RegisterComponents()
    {
        ComponentRegistry.RegisterComponent<TransformComponent>("Transform",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& transform = reg.get<TransformComponent>(entity);
                ImGui::DragFloat3("Translation", &transform.Translation.x, 0.1f);

                glm::vec3 rotation = transform.GetRotationEuler();
                glm::vec3 rotationDegrees = glm::degrees(rotation);
                if (ImGui::DragFloat3("Rotation", &rotationDegrees.x, 0.1f))
                {
                    glm::vec3 newRotation = glm::radians(rotationDegrees);
                    transform.SetRotationEuler(newRotation);
                }

                ImGui::DragFloat3("Scale", &transform.Scale.x, 0.1f);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& transform = reg.get<TransformComponent>(entity);
                json["Translation"] = { transform.Translation.x, transform.Translation.y, transform.Translation.z };
                json["Rotation"] = { transform.Rotation.w, transform.Rotation.x, transform.Rotation.y, transform.Rotation.z };
                json["Scale"] = { transform.Scale.x, transform.Scale.y, transform.Scale.z };
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& transform = reg.get_or_emplace<TransformComponent>(entity);
                auto trans = json["Translation"];
                transform.Translation = glm::vec3(trans[0], trans[1], trans[2]);
                auto rot = json["Rotation"];
                transform.Rotation = glm::quat(rot[0], rot[1], rot[2], rot[3]);
                auto scale = json["Scale"];
                transform.Scale = glm::vec3(scale[0], scale[1], scale[2]);
            });

        ComponentRegistry.RegisterComponent<TagComponent>("Tag",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& tag = reg.get<TagComponent>(entity).Tag;
                char buffer[256];
                memset(buffer, 0, sizeof(buffer));
                strcpy_s(buffer, sizeof(buffer), tag.c_str());
                if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
                {
                    tag = std::string(buffer);
                }
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& tagComponent = reg.get<TagComponent>(entity);
                json["Tag"] = tagComponent.Tag;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& tagComponent = reg.get_or_emplace<TagComponent>(entity);
                auto tag = json["Tag"];
                tagComponent.Tag = tag;
            });

        ComponentRegistry.RegisterComponent<CameraComponent>("Camera",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& cameraComp = reg.get<CameraComponent>(entity);
                ImGui::Checkbox("Primary", &cameraComp.Primary);
                ImGui::Checkbox("FixedAspectRatio", &cameraComp.FixedAspectRatio);

                const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
                const char* currentProjectionTypeStr = projectionTypeStrings[(int)cameraComp.Camera.GetProjectionType()];

                if (ImGui::BeginCombo("Projection", currentProjectionTypeStr))
                {
                    for (int i = 0; i < 2; i++)
                    {
                        bool isSelected = currentProjectionTypeStr == projectionTypeStrings[i];
                        if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
                        {
                            currentProjectionTypeStr = projectionTypeStrings[i];
                            cameraComp.Camera.SetProjectionType((SceneCamera::ProjectionType)i);
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (cameraComp.Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
                {
                    float currFOV = glm::degrees(cameraComp.Camera.GetPerspectiveVerticalFOV());
                    float currNear = cameraComp.Camera.GetPerspectiveNearClip();
                    float currFar = cameraComp.Camera.GetPerspectiveFarClip();
                    if (ImGui::DragFloat("FOV", &currFOV, 0.1f))
                    {
                        cameraComp.Camera.SetPerspectiveVerticalFOV(glm::radians(currFOV));
                    }
                    if (ImGui::DragFloat("Near Clip", &currNear, 0.1f))
                    {
                        cameraComp.Camera.SetPerspectiveNearClip(currNear);
                    }
                    if (ImGui::DragFloat("Far Clip", &currFar, 0.1f))
                    {
                        cameraComp.Camera.SetPerspectiveFarClip(currFar);
                    }
                }
                else
                {
                    float currSize = cameraComp.Camera.GetOrthographicSize();
                    float currNear = cameraComp.Camera.GetOrthographicNearClip();
                    float currFar = cameraComp.Camera.GetOrthographicFarClip();
                    if (ImGui::DragFloat("Size", &currSize, 0.1f))
                    {
                        cameraComp.Camera.SetOrthographicSize(currSize);
                    }
                    if (ImGui::DragFloat("Near Clip", &currNear, 0.1f))
                    {
                        cameraComp.Camera.SetOrthographicNearClip(currNear);
                    }
                    if (ImGui::DragFloat("Far Clip", &currFar, 0.1f))
                    {
                        cameraComp.Camera.SetOrthographicFarClip(currFar);
                    }
                }
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& camComp = reg.get<CameraComponent>(entity);

                auto sceneCamObj = nlohmann::json::object();
                sceneCamObj["ProjectionType"] = camComp.Camera.GetProjectionType();
                sceneCamObj["PerspectiveFOV"] = camComp.Camera.GetPerspectiveVerticalFOV();
                sceneCamObj["PerspectiveNear"] = camComp.Camera.GetPerspectiveNearClip();
                sceneCamObj["PerspectiveFar"] = camComp.Camera.GetPerspectiveFarClip();
                sceneCamObj["OrthographicSize"] = camComp.Camera.GetOrthographicSize();
                sceneCamObj["OrthographicNear"] = camComp.Camera.GetOrthographicNearClip();
                sceneCamObj["OrthographicFar"] = camComp.Camera.GetOrthographicFarClip();
                json["SceneCamera"] = sceneCamObj;
                json["Primary"] = camComp.Primary;
                json["FixedAspectRatio"] = camComp.FixedAspectRatio;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& camComp = reg.get_or_emplace<CameraComponent>(entity);
                camComp.Primary = json["Primary"];
                camComp.FixedAspectRatio = json["FixedAspectRatio"];
                const auto& sceneCamObj = json["SceneCamera"];
                const auto& projectionType = sceneCamObj["ProjectionType"];
                if (projectionType == SceneCamera::ProjectionType::Perspective)
                {
                    camComp.Camera.SetPerspective(
                        sceneCamObj["PerspectiveFOV"],
                        sceneCamObj["PerspectiveNear"],
                        sceneCamObj["PerspectiveFar"]
                    );
                }
                else
                {
                    camComp.Camera.SetOrthographic(
                        sceneCamObj["OrthographicSize"],
                        sceneCamObj["OrthographicNear"],
                        sceneCamObj["OrthographicFar"]
                    );
                }
            });

        ComponentRegistry.RegisterComponent<MeshComponent>("Mesh",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& meshComp = reg.get<MeshComponent>(entity);

                // TODO: Add asset handle
                ImGui::ColorEdit4("Color", &meshComp.Color[0]);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& meshComp = reg.get<MeshComponent>(entity);
                std::string assetPath = Engine::Get().GetAssetManager().GetAssetPath(meshComp.Mesh);
                json["Mesh"] = assetPath;
                json["Color"] = { meshComp.Color.r, meshComp.Color.g, meshComp.Color.b, meshComp.Color.a };
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& meshComp = reg.get_or_emplace<MeshComponent>(entity);
                std::string assetPath = json["Mesh"];
                if (assetPath == "")
                    meshComp.Mesh = AssetHandle::Null();
                else if (assetPath == "DEFAULT_CUBE")
                    meshComp.Mesh = Engine::Get().GetAssetManager().GetDefaultCube()->GetHandle();
                else
                    meshComp.Mesh = Engine::Get().GetAssetManager().GetMesh(assetPath)->GetHandle();

                const auto& color = json["Color"];
                meshComp.Color = glm::vec4(color[0], color[1], color[2], color[3]);
            });

        ComponentRegistry.RegisterComponent<RigidBodyComponent>("RigidBody",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& rbComp = reg.get<RigidBodyComponent>(entity);

                const char* rbTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
                const char* currentRbTypeStr = rbTypeStrings[(int)rbComp.Type];

                if (ImGui::BeginCombo("Body Type", currentRbTypeStr))
                {
                    for (int i = 0; i < 2; i++)
                    {
                        bool isSelected = currentRbTypeStr == rbTypeStrings[i];
                        if (ImGui::Selectable(rbTypeStrings[i], isSelected))
                        {
                            currentRbTypeStr = rbTypeStrings[i];
                            rbComp.Type = (RigidBodyType)i;
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::Checkbox("LockRotationX", &rbComp.LockRotationX);
                ImGui::Checkbox("LockRotationY", &rbComp.LockRotationY);
                ImGui::Checkbox("LockRotationZ", &rbComp.LockRotationZ);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& rbComp = reg.get<RigidBodyComponent>(entity);
                json["Type"] = rbComp.Type;
                json["LockRotationX"] = rbComp.LockRotationX;
                json["LockRotationY"] = rbComp.LockRotationY;
                json["LockRotationZ"] = rbComp.LockRotationZ;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& rbComp = reg.get_or_emplace<RigidBodyComponent>(entity);
                rbComp.Type = json["Type"];
                rbComp.LockRotationX = json["LockRotationX"];
                rbComp.LockRotationY = json["LockRotationY"];
                rbComp.LockRotationZ = json["LockRotationZ"];
            });

        ComponentRegistry.RegisterComponent<BoxColliderComponent>("BoxCollider",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& bcComp = reg.get<BoxColliderComponent>(entity);
                ImGui::DragFloat3("Half Size", &bcComp.HalfSize.x);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& bcComp = reg.get<BoxColliderComponent>(entity);
                json["HalfSize"] = {bcComp.HalfSize.x, bcComp.HalfSize.y, bcComp.HalfSize.z };
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& bcComp = reg.get_or_emplace<BoxColliderComponent>(entity);
                const auto& halfSize = json["HalfSize"];
                bcComp.HalfSize = glm::vec3(halfSize[0], halfSize[1], halfSize[2]);
            });

        ComponentRegistry.RegisterComponent<SphereColliderComponent>("SphereCollider",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& scComp = reg.get<SphereColliderComponent>(entity);
                ImGui::DragFloat("Radius", &scComp.Radius);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& scComp = reg.get<SphereColliderComponent>(entity);
                json["Radius"] = scComp.Radius;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& scComp = reg.get_or_emplace<SphereColliderComponent>(entity);
                scComp.Radius = json["Radius"];
            });

        ComponentRegistry.RegisterComponent<CapsuleColliderComponent>("CapsuleCollider",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& ccComp = reg.get<CapsuleColliderComponent>(entity);
                ImGui::DragFloat("Radius", &ccComp.Radius);
                ImGui::DragFloat("Height", &ccComp.Height);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& ccComp = reg.get<CapsuleColliderComponent>(entity);
                json["Radius"] = ccComp.Radius;
                json["Height"] = ccComp.Height;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& ccComp = reg.get_or_emplace<CapsuleColliderComponent>(entity);
                ccComp.Radius = json["Radius"];
                ccComp.Height = json["Height"];
            });
    }
}
