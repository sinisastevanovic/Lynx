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

#include "ScriptRegistry.h"
#include "Event/ActionEvent.h"
#include "ImGui/EditorUIHelpers.h"
#include "Scene/Components/LuaScriptComponent.h"
#include "Scene/Components/NativeScriptComponent.h"


namespace Lynx
{
    Engine* Engine::s_Instance = nullptr;
    
    void Engine::Initialize(bool editorMode)
    {
        s_Instance = this;
        Log::Init();
        LX_CORE_INFO("Initializing...");

        RegisterScripts();
        RegisterComponents();

        m_Window = Window::Create();
        m_Window->SetEventCallback([this](Event& event){ this->OnEvent(event); });

        m_AssetRegistry = std::make_unique<AssetRegistry>();
        m_AssetRegistry->LoadRegistry("assets", "engine/resources");
        m_AssetManager = std::make_unique<AssetManager>(m_AssetRegistry.get());
        
        m_EditorCamera = EditorCamera(45.0f, (float)m_Window->GetWidth() / (float)m_Window->GetHeight(), 0.1f, 1000.0f);

        m_PhysicsSystem = std::make_unique<PhysicsSystem>();
        
        m_Renderer = std::make_unique<Renderer>(m_Window->GetNativeWindow(), editorMode);
        m_Renderer->Init();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(m_Window->GetNativeWindow(), true);
        m_Renderer->InitImGui();

        m_ScriptEngine = std::make_unique<ScriptEngine>();
        
        m_Scene = std::make_shared<Scene>();
        
        m_Window->SetVSync(true);

    }

    void Engine::Run(IGameModule* gameModule)
    {
        LX_CORE_INFO("Starting main loop...");

        if (gameModule)
        {
            gameModule->RegisterScripts();
            gameModule->RegisterComponents(&ComponentRegistry);
            gameModule->OnStart();
        }
        auto cubeMesh = m_AssetManager->GetDefaultCube();

        if (m_Scene && m_SceneState == SceneState::Play)
            m_Scene->OnRuntimeStart();

        if (m_Scene && m_SceneState == SceneState::Edit)
            m_ScriptEngine->OnEditorStart(m_Scene.get(), true);

        // Load all textures and meshes
        // TODO: This is temporary code, replace with real scene loading logic
        auto view = m_Scene->Reg().view<MeshComponent>();
        for (auto entity : view)
        {
            auto& mesh = view.get<MeshComponent>(entity);
            if (mesh.Mesh)
            {
                auto meshasset = m_AssetManager->GetAsset<StaticMesh>(mesh.Mesh);
                if (meshasset)
                {
                    for (const auto& subMesh : meshasset->GetSubmeshes())
                    {
                        if (subMesh.Material->AlbedoTexture)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->AlbedoTexture);
                        if (subMesh.Material->NormalMap)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->NormalMap);
                        if (subMesh.Material->MetallicRoughnessTexture)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->MetallicRoughnessTexture);
                        if (subMesh.Material->EmissiveTexture)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->EmissiveTexture);
                    }
                }
            }
        }

        float lastFrameTime = (float)glfwGetTime();
        // This is a placeholder for the real game loop
        while (m_IsRunning)
        {
            float time = (float)glfwGetTime();
            float deltaTime = time - lastFrameTime;
            lastFrameTime = time;
            
            if (m_Window->ShouldClose())
                m_IsRunning = false;
            
            m_Window->OnUpdate();

            m_AssetManager->Update();

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

                        cameraPos = transform.Translation;

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
                cameraPos = m_EditorCamera.GetPosition();
            }

            // TODO: We should load all the textures and meshes before we run the scene!!!
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

            glm::vec3 lightDir = { -0.5f, -0.7f, 1.0f };
            glm::vec3 lightColor = { 1.0f, 1.0f, 1.0f };
            float lightIntensity = 1.0f;

            auto sunView = m_Scene->Reg().view<TransformComponent, DirectionalLightComponent>();
            for (auto entity : sunView)
            {
                auto [transform, light] = sunView.get<TransformComponent, DirectionalLightComponent>(entity);
                lightDir = glm::rotate(transform.Rotation, glm::vec3(0.0f, 0.0f, -1.0f));
                lightColor = light.Color;
                lightIntensity = light.Intensity;
                break;
            }
            
            m_Renderer->BeginScene(cameraViewProj, cameraPos, lightDir, lightColor, lightIntensity);
            auto view = m_Scene->Reg().view<TransformComponent, MeshComponent>();
            for (auto entity : view)
            {
                auto [transform, meshComp] = view.get<TransformComponent, MeshComponent>(entity);
                auto mesh = m_AssetManager->GetAsset<StaticMesh>(meshComp.Mesh);
                m_Renderer->SubmitMesh(mesh, transform.GetTransform(), meshComp.Color, (int)entity);
            }

            if (false && m_SceneState == SceneState::Edit) // Render collider meshes
            {
                auto colliderView = m_Scene->Reg().view<TransformComponent, BoxColliderComponent>();

                for (auto entity : colliderView)
                {
                    auto [transform, collider] = colliderView.get<TransformComponent, BoxColliderComponent>(entity);
                    // We calculate a transform that matches the collider's bounds
                    // BoxCollider::HalfSize * 2 = Full Scale
                    glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), transform.Translation)
                        * glm::toMat4(transform.Rotation)
                        * glm::scale(glm::mat4(1.0f), collider.HalfSize * 2.0f);

                    m_Renderer->SubmitMesh(cubeMesh, colliderTransform, glm::vec4(0.0f, 1.0f, 0.0f, 0.5f), (int)entity);
                }
            }

            m_Renderer->EndScene();
        }

        if (m_Scene && m_SceneState == SceneState::Play)
            m_Scene->OnRuntimeStop();

        if (gameModule)
        {
            gameModule->OnShutdown();
        }
    }

    void Engine::Shutdown()
    {
        LX_CORE_INFO("Shutting down...");
        if (m_SceneState == SceneState::Play && m_Scene)
            m_Scene->OnRuntimeStop();

        m_Scene.reset();
        
        m_ScriptEngine.reset();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_AssetManager.reset();
        m_Renderer.reset();
        m_PhysicsSystem.reset();
        Log::Shutdown();
    }

    void Engine::SetSceneState(SceneState state)
    {
        m_SceneState = state;
        if (m_SceneState == SceneState::Play)
            m_Scene->OnRuntimeStart();
        else if (m_SceneState == SceneState::Edit)
            m_Scene->OnRuntimeStop();
    }

    void Engine::OnEvent(Event& e)
    {
        m_EditorCamera.OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& event)
        {
            if (m_SceneState == SceneState::Play)
            {
                ImGuiIO& io = ImGui::GetIO();
                if (io.WantCaptureKeyboard || m_BlockEvents)
                    return false;

                std::string action = Input::GetActionFromKey(event.GetKeyCode());
                if (!action.empty())
                {
                    ActionPressedEvent e(action);
                    this->OnEvent(e);
                    m_ScriptEngine->OnActionEvent(action, true);
                }
            }
            
            if (event.GetKeyCode() == KeyCode::Escape)
            {
                LX_CORE_WARN("Escape key pressed via event, closing application.");
                m_IsRunning = false;
                return true; 
            }
            return false;
        });

        dispatcher.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& event)
        {
            if (m_SceneState == SceneState::Play)
            {
                ImGuiIO& io = ImGui::GetIO();
                if (io.WantCaptureKeyboard || m_BlockEvents)
                    return false;
                
                std::string action = Input::GetActionFromKey(event.GetKeyCode());
                if (!action.empty())
                {
                    ActionReleasedEvent e(action);
                    this->OnEvent(e);
                    m_ScriptEngine->OnActionEvent(action, false);
                }
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

    void Engine::RegisterScripts()
    {
        
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
                /*std::string meshName = Engine::Get().GetAssetManager().GetAssetName(meshComp.Mesh);
                char buffer[256];
                memset(buffer, 0, sizeof(buffer));
                strcpy_s(buffer, sizeof(buffer), meshName.c_str());
                ImGui::InputText("Static Mesh", buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetUtils::GetDragDropPayload(AssetType::StaticMesh)))
                    {
                        uint64_t data = *(const uint64_t*)payload->Data;
                        meshComp.Mesh = AssetHandle(data);
                    }
                    ImGui::EndDragDropTarget();
                }*/
                EditorUIHelpers::DrawAssetSelection("Static Mesh", meshComp.Mesh, AssetType::StaticMesh);
                ImGui::ColorEdit4("Color", &meshComp.Color[0]);

                
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& meshComp = reg.get<MeshComponent>(entity);
                json["Mesh"] = (uint64_t)meshComp.Mesh;
                json["Color"] = { meshComp.Color.r, meshComp.Color.g, meshComp.Color.b, meshComp.Color.a };
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& meshComp = reg.get_or_emplace<MeshComponent>(entity);
                meshComp.Mesh = AssetHandle(json["Mesh"]);
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

        ComponentRegistry.RegisterComponent<NativeScriptComponent>("NativeScript",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& nscComp = reg.get<NativeScriptComponent>(entity);

                std::string current = nscComp.ScriptName.empty() ? "None" : nscComp.ScriptName;
                if (ImGui::BeginCombo("Script Class", current.c_str()))
                {
                    for (const auto& [name, bindFunc] : ScriptRegistry::GetRegisteredScripts())
                    {
                        bool isSelected = current == name;
                        if (ImGui::Selectable(name.c_str(), isSelected))
                        {
                            nscComp.ScriptName = name;
                            ScriptRegistry::Bind(name, nscComp);
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& nscComp = reg.get<NativeScriptComponent>(entity);
                json["ScriptName"] = nscComp.ScriptName;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& nscComp = reg.get<NativeScriptComponent>(entity);
                if (json.contains("ScriptName"))
                {
                    std::string scriptName = json["ScriptName"];
                    ScriptRegistry::Bind(scriptName, nscComp);
                }
            });

        ComponentRegistry.RegisterComponent<LuaScriptComponent>("LuaScript",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& lscComp = reg.get<LuaScriptComponent>(entity);
                EditorUIHelpers::DrawAssetSelection("Script", lscComp.ScriptHandle, AssetType::Script);
                EditorUIHelpers::DrawLuaScriptSection(&lscComp);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& lscComp = reg.get<LuaScriptComponent>(entity);
                json["Script"] = (uint64_t)lscComp.ScriptHandle;

                if (lscComp.Self.valid())
                {
                    sol::optional<sol::table> propsOpt = lscComp.Self["Properties"];
                    if (propsOpt)
                    {
                        auto propsJson = nlohmann::json::object();
                        sol::table propsDef = propsOpt.value();

                        for (auto& [key, value] : propsDef)
                        {
                            std::string name = key.as<std::string>();
                            sol::table def = value.as<sol::table>();
                            std::string type = def["Type"];

                            sol::object runtimeVal = lscComp.Self[name];
                            if (!runtimeVal.valid())
                                continue;
                            
                            if (type == "Float") propsJson[name] = runtimeVal.as<float>();
                            else if (type == "Int") propsJson[name] = runtimeVal.as<int>();
                            else if (type == "String") propsJson[name] = runtimeVal.as<std::string>();
                            else if (type == "Bool") propsJson[name] = runtimeVal.as<bool>();
                            else if (type == "Vec2")
                            {
                                glm::vec2 vec = runtimeVal.as<glm::vec2>();
                                propsJson[name] = {vec.x, vec.y};
                            }
                            else if (type == "Vec3")
                            {
                                glm::vec3 vec = runtimeVal.as<glm::vec3>();
                                propsJson[name] = {vec.x, vec.y, vec.z};
                            }
                            else if (type == "Color")
                            {
                                glm::vec4 vec = runtimeVal.as<glm::vec4>();
                                propsJson[name] = {vec.r, vec.g, vec.b, vec.a};
                            }
                        }

                        json["Properties"] = propsJson;
                    }
                }
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& lscComp = reg.get<LuaScriptComponent>(entity);
                lscComp.ScriptHandle = AssetHandle(json["Script"]);

                if (!lscComp.Self.valid() && lscComp.ScriptHandle.IsValid())
                {
                    auto scene = Engine::Get().GetActiveScene();
                    if (scene)
                    {
                        Entity e { entity, scene.get() };
                        Engine::Get().GetScriptEngine()->LoadScript(e);
                    }
                }

                if (!json.contains("Properties"))
                    return;
                
                const auto& propsJson = json["Properties"];

                std::optional<sol::table> propsOpt = lscComp.Self["Properties"];
                if (propsOpt)
                {
                    sol::table propsDef = propsOpt.value();
                    for (auto& [key, value] : propsDef)
                    {
                        std::string name = key.as<std::string>();
                        if (!propsJson.contains(name))
                            continue;
                        sol::table def = value.as<sol::table>();
                        std::string type = def["Type"];
                        if (type == "Float") lscComp.Self[name] = propsJson[name].get<float>();
                        else if (type == "Int") lscComp.Self[name] = propsJson[name].get<int>();
                        else if (type == "String") lscComp.Self[name] = propsJson[name].get<std::string>();
                        else if (type == "Bool") lscComp.Self[name] = propsJson[name].get<bool>();
                        else if (type == "Vec2")
                        {
                            const auto& jsonVal = propsJson[name];
                            lscComp.Self[name] = glm::vec2(jsonVal[0], jsonVal[1]);
                        }
                        else if (type == "Vec3")
                        {
                            const auto& jsonVal = propsJson[name];
                            lscComp.Self[name] = glm::vec3(jsonVal[0], jsonVal[1], jsonVal[2]);
                        }
                        else if (type == "Color")
                        {
                            const auto& jsonVal = propsJson[name];
                            lscComp.Self[name] = glm::vec4(jsonVal[0], jsonVal[1], jsonVal[2], jsonVal[3]);
                        }
                    }
                }
            });

        ComponentRegistry.RegisterComponent<DirectionalLightComponent>("DirectionalLight",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& light = reg.get<DirectionalLightComponent>(entity);
                ImGui::ColorEdit3("Color", &light.Color.x);
                ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 100.0f);
                ImGui::Checkbox("Cast Shadows", &light.CastShadows);
            },
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& light = reg.get<DirectionalLightComponent>(entity);
                json["Color"] = { light.Color.r, light.Color.g, light.Color.b };
                json["Intensity"] = light.Intensity;
                json["CastShadows"] = light.CastShadows;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& light = reg.get<DirectionalLightComponent>(entity);
                const auto& color = json["Color"];
                light.Color = glm::vec3(color[0], color[1], color[2]);
                light.Intensity = json["Intensity"];
                light.CastShadows = json["CastShadows"];
            });
    }
}
