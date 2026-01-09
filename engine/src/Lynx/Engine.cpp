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
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "Event/ActionEvent.h"
#include "Event/SceneEvents.h"
#include "ImGui/EditorUIHelpers.h"
#include "Renderer/DebugRenderer.h"
#include "Scene/Components/LuaScriptComponent.h"
#include "Scene/Components/NativeScriptComponent.h"
#include "Scene/Components/UIComponents.h"


namespace Lynx
{
    Engine* Engine::s_Instance = nullptr;
    
    void Engine::Initialize(bool editorMode)
    {
        s_Instance = this;
        m_IsEditor = editorMode;
        Log::Init();
        LX_CORE_INFO("Initializing...");

        RegisterCoreScripts();
        RegisterCoreComponents();

        m_Window = Window::Create();
        m_Window->SetEventCallback([this](Event& event){ this->OnEvent(event); });

        m_AssetRegistry = std::make_unique<AssetRegistry>();
        m_AssetRegistry->LoadRegistry("assets", "engine/resources");
        m_AssetManager = std::make_unique<AssetManager>(m_AssetRegistry.get());
        
        m_EditorCamera = EditorCamera(45.0f, (float)m_Window->GetWidth() / (float)m_Window->GetHeight(), 0.1f, 1000.0f);

        m_Renderer = std::make_unique<Renderer>(m_Window->GetNativeWindow(), editorMode);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ImGui_ImplGlfw_InitForVulkan(m_Window->GetNativeWindow(), true);
        ImGui::StyleColorsDark();
        m_Renderer->Init();

        m_ScriptEngine = std::make_unique<ScriptEngine>();
        
        m_Scene = std::make_shared<Scene>();
        m_SceneRenderer = std::make_unique<SceneRenderer>(m_Scene);
        
        m_Window->SetVSync(true);

    }

    void Engine::Run(IGameModule* gameModule)
    {
        LX_CORE_INFO("Starting main loop...");
        
        ViewportResizeEvent e(m_Window->GetWidth(), m_Window->GetHeight());
        OnEvent(e);

        if (gameModule)
        {
            m_GameModule = gameModule;
            GameTypeRegistry gameReg(m_ComponentRegistry);
            gameModule->RegisterScripts(&gameReg);
            gameModule->RegisterComponents(&gameReg);
        }
        auto cubeMesh = m_AssetManager->GetDefaultCube();

        if (m_Scene && m_SceneState == SceneState::Play)
            SetSceneState(SceneState::Play);

        if (m_Scene && m_SceneState == SceneState::Edit)
        {
            m_ScriptEngine->OnEditorStart(m_Scene.get(), true);
        }

        // Load all textures and meshes
        // TODO: This is temporary code, replace with real scene loading logic
        auto view = m_Scene->Reg().view<MeshComponent>();
        for (auto entity : view)
        {
            auto& mesh = view.get<MeshComponent>(entity);
            if (mesh.Mesh)
            {
                auto meshasset = m_AssetManager->GetAsset<StaticMesh>(mesh.Mesh, AssetLoadMode::Blocking);
                if (meshasset)
                {
                    for (const auto& subMesh : meshasset->GetSubmeshes())
                    {
                        if (subMesh.Material->AlbedoTexture)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->AlbedoTexture, AssetLoadMode::Blocking);
                        if (subMesh.Material->NormalMap)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->NormalMap, AssetLoadMode::Blocking);
                        if (subMesh.Material->MetallicRoughnessTexture)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->MetallicRoughnessTexture, AssetLoadMode::Blocking);
                        if (subMesh.Material->EmissiveTexture)
                            m_AssetManager->GetAsset<Texture>(subMesh.Material->EmissiveTexture, AssetLoadMode::Blocking);
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

            auto viewportSize = m_Renderer->GetViewportSize();
            if (m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0 || viewportSize.first == 0 || viewportSize.second == 0)
                continue;

            m_AssetManager->Update();

            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (m_ImGuiCallback)
                m_ImGuiCallback();

            ImGui::Render();

            // TODO: We should load all the textures and meshes before we run the scene!!!
            if (m_SceneState == SceneState::Edit)
            {
                m_EditorCamera.OnUpdate(deltaTime);
                m_Scene->OnUpdateEditor(deltaTime, m_EditorCamera.GetPosition());
                
                m_SceneRenderer->RenderEditor(m_EditorCamera, deltaTime);
            }
            else if (m_SceneState == SceneState::Play)
            {
                m_Scene->OnUpdateRuntime(deltaTime);
                if (gameModule)
                    gameModule->OnUpdate(deltaTime);
                
                m_Scene->UpdateGlobalTransforms();
                m_SceneRenderer->RenderRuntime(deltaTime);
            }
        }

        if (m_Scene && m_SceneState == SceneState::Play)
        {
            m_Scene->OnRuntimeStop();
            if (gameModule)
                gameModule->OnShutdown();
        }
    }

    void Engine::Shutdown()
    {
        LX_CORE_INFO("Shutting down...");
        if (m_SceneState == SceneState::Play && m_Scene)
            m_Scene->OnRuntimeStop();

        m_SceneRenderer.reset();
        m_Scene.reset();
        
        m_ScriptEngine.reset();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_AssetManager.reset();
        m_Renderer.reset();
        Log::Shutdown();
    }

    void Engine::ClearGameTypes()
    {
        m_ComponentRegistry.ClearGameComponents();
        m_ComponentRegistry.ClearGameScripts();
    }

    void Engine::SetActiveScene(std::shared_ptr<Scene> scene)
    {
        if (m_Scene && m_SceneState == SceneState::Play)
        {
            m_Scene->OnRuntimeStop();
        }
        m_Scene = scene;
        if (m_SceneRenderer)
            m_SceneRenderer->SetScene(m_Scene);
        
        SetSceneState(m_SceneState);
        ActiveSceneChangedEvent e(m_Scene);
        OnEvent(e);
    }

    void Engine::ClearActiveScene()
    {
        m_Scene.reset();
        m_Scene = std::make_shared<Scene>();
        SetActiveScene(m_Scene);
    }

    void Engine::SetSceneState(SceneState state)
    {
        if (m_SceneState == SceneState::Play && state == SceneState::Edit && m_GameModule)
            m_GameModule->OnShutdown();
        
        m_SceneState = state;
        if (m_SceneRenderer)
            m_SceneRenderer->SetViewportDirty(true);
        if (m_SceneState == SceneState::Play)
        {
            m_Scene->OnRuntimeStart();
            if (m_GameModule)
                m_GameModule->OnStart();
        }
        else if (m_SceneState == SceneState::Edit)
        {
            m_Scene->OnRuntimeStop();
            m_ScriptEngine->OnEditorStart(m_Scene.get());
        }
        
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
            if (!m_IsEditor)
            {
                ViewportResizeEvent e(event.GetWidth(), event.GetHeight());
                OnEvent(e);
            }
            return false;
        });
        
        dispatcher.Dispatch<ViewportResizeEvent>([this](ViewportResizeEvent& event)
        {
            //m_Renderer->EnsureEditorViewport(event.GetWidth(), event.GetHeight());
            m_SceneRenderer->SetViewportSize(event.GetWidth(), event.GetHeight());
            return false;
        });

        if (m_AppEventCallback)
        {
            m_AppEventCallback(e);
            if (e.Handled)
                return;
        }
        else if (m_Scene)
        {
            m_Scene->OnEvent(e);
        }
    }

    void Engine::RegisterCoreScripts()
    {
        
    }

    void Engine::RegisterCoreComponents()
    {
        m_ComponentRegistry.RegisterCoreComponent<TransformComponent>("Transform",
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
            },
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
            });

        m_ComponentRegistry.RegisterCoreComponent<RelationshipComponent>("Relationship",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& comp = reg.get<RelationshipComponent>(entity);
                if (comp.Parent != entt::null)
                {
                    if (reg.all_of<IDComponent>(comp.Parent))
                    {
                        json["Parent"] = (uint64_t)reg.get<IDComponent>(comp.Parent).ID;
                    }
                }
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                
            });

        m_ComponentRegistry.RegisterCoreComponent<TagComponent>("Tag",
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
            },
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
            });

        m_ComponentRegistry.RegisterCoreComponent<CameraComponent>("Camera",
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
            },
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
            });

        m_ComponentRegistry.RegisterCoreComponent<MeshComponent>("Mesh",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& meshComp = reg.get<MeshComponent>(entity);
                json["Mesh"] = (uint64_t)meshComp.Mesh;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& meshComp = reg.get_or_emplace<MeshComponent>(entity);
                meshComp.Mesh = AssetHandle(json["Mesh"]);
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& meshComp = reg.get<MeshComponent>(entity);
               
                EditorUIHelpers::DrawAssetSelection("Static Mesh", meshComp.Mesh, {AssetType::StaticMesh});
                if (meshComp.Mesh)
                {
                    auto mesh = Engine::Get().GetAssetManager().GetAsset<StaticMesh>(meshComp.Mesh);
                    if (mesh)
                    {
                        auto& submeshes = mesh->GetSubmeshes();
                        int index = 0;
                        for (auto& submesh : submeshes)
                        {
                            if (submesh.Material)
                            {
                                std::string matId = "Material " + std::to_string(index);
                                ImGui::PushID(matId.c_str());

                                ImGui::Text(matId.c_str());
                                ImGui::ColorEdit4("Albedo Color", &submesh.Material->AlbedoColor.r);
                                ImGui::DragFloat("Metallic", &submesh.Material->Metallic);
                                ImGui::DragFloat("Roughness", &submesh.Material->Roughness);
                                ImGui::ColorEdit3("Emissive Color", &submesh.Material->EmissiveColor.r);
                                ImGui::DragFloat("Emissive Strength", &submesh.Material->EmissiveStrength);
                                ImGui::Separator();
                                
                                ImGui::PopID();
                            }
                            index++;
                        }
                    }
                }
            });

        m_ComponentRegistry.RegisterCoreComponent<RigidBodyComponent>("RigidBody",
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
            },
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
            });

        m_ComponentRegistry.RegisterCoreComponent<BoxColliderComponent>("BoxCollider",
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
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& bcComp = reg.get<BoxColliderComponent>(entity);
                ImGui::DragFloat3("Half Size", &bcComp.HalfSize.x);
            });

        m_ComponentRegistry.RegisterCoreComponent<SphereColliderComponent>("SphereCollider",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& scComp = reg.get<SphereColliderComponent>(entity);
                json["Radius"] = scComp.Radius;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& scComp = reg.get_or_emplace<SphereColliderComponent>(entity);
                scComp.Radius = json["Radius"];
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& scComp = reg.get<SphereColliderComponent>(entity);
                ImGui::DragFloat("Radius", &scComp.Radius);
            });

        m_ComponentRegistry.RegisterCoreComponent<CapsuleColliderComponent>("CapsuleCollider",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& ccComp = reg.get<CapsuleColliderComponent>(entity);
                json["Radius"] = ccComp.Radius;
                json["HalfHeight"] = ccComp.HalfHeight;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& ccComp = reg.get_or_emplace<CapsuleColliderComponent>(entity);
                ccComp.Radius = json["Radius"];
                ccComp.HalfHeight = json["HalfHeight"];
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& ccComp = reg.get<CapsuleColliderComponent>(entity);
                ImGui::DragFloat("Radius", &ccComp.Radius);
                ImGui::DragFloat("HalfHeight", &ccComp.HalfHeight);
            });

        m_ComponentRegistry.RegisterCoreComponent<NativeScriptComponent>("NativeScript",
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
                    Engine::Get().GetComponentRegistry().BindScript(scriptName, nscComp);
                }
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& nscComp = reg.get<NativeScriptComponent>(entity);

                std::string current = nscComp.ScriptName.empty() ? "None" : nscComp.ScriptName;
                if (ImGui::BeginCombo("Script Class", current.c_str()))
                {
                    for (const auto& [name, bindFunc] : Engine::Get().GetComponentRegistry().GetRegisteredScripts())
                    {
                        bool isSelected = current == name;
                        if (ImGui::Selectable(name.c_str(), isSelected))
                        {
                            nscComp.ScriptName = name;
                            Engine::Get().GetComponentRegistry().BindScript(name, nscComp);
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            });

        m_ComponentRegistry.RegisterCoreComponent<LuaScriptComponent>("LuaScript",
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
                    Scene* scene = reg.ctx().get<Scene*>();
                    if (scene)
                    {
                        Entity e { entity, scene };
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
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& lscComp = reg.get<LuaScriptComponent>(entity);
                EditorUIHelpers::DrawAssetSelection("Script", lscComp.ScriptHandle, {AssetType::Script});
                EditorUIHelpers::DrawLuaScriptSection(&lscComp);
            });

        m_ComponentRegistry.RegisterCoreComponent<DirectionalLightComponent>("DirectionalLight",
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
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& light = reg.get<DirectionalLightComponent>(entity);
                ImGui::ColorEdit3("Color", &light.Color.x);
                ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 100.0f);
                ImGui::Checkbox("Cast Shadows", &light.CastShadows);
            });

        m_ComponentRegistry.RegisterCoreComponent<ParticleEmitterComponent>("ParticleEmitter",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& comp = reg.get<ParticleEmitterComponent>(entity);
                json["Material"] = (uint64_t)comp.Material;
                json["MaxParticles"] = comp.MaxParticles;
                json["EmissionRate"] = comp.EmissionRate;
                json["IsLooping"] = comp.IsLooping;
                json["DepthSorting"] = comp.DepthSorting;

                auto props = nlohmann::json::object();
                props["Position"] = { comp.Properties.Position.x, comp.Properties.Position.y, comp.Properties.Position.z };
                props["Velocity"] = { comp.Properties.Velocity.x, comp.Properties.Velocity.y, comp.Properties.Velocity.z };
                props["VelocityVariation"] = { comp.Properties.VelocityVariation.x, comp.Properties.VelocityVariation.y, comp.Properties.VelocityVariation.z };
                props["ColorBegin"] = { comp.Properties.ColorBegin.r, comp.Properties.ColorBegin.g, comp.Properties.ColorBegin.b, comp.Properties.ColorBegin.a };
                props["ColorEnd"] = { comp.Properties.ColorEnd.r, comp.Properties.ColorEnd.g, comp.Properties.ColorEnd.b, comp.Properties.ColorEnd.a };
                props["SizeBegin"] = comp.Properties.SizeBegin;
                props["SizeEnd"] = comp.Properties.SizeEnd;
                props["SizeVariation"] = comp.Properties.SizeVariation;
                props["LifeTime"] = comp.Properties.LifeTime;

                json["Properties"] = props;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& comp = reg.get_or_emplace<ParticleEmitterComponent>(entity);
                if (json.contains("Material")) comp.Material = AssetHandle(json["Material"]);
                if (json.contains("MaxParticles"))
                {
                    comp.SetMaxParticles(json["MaxParticles"]);
                }
                if (json.contains("EmissionRate")) comp.EmissionRate = json["EmissionRate"];
                if (json.contains("IsLooping")) comp.IsLooping = json["IsLooping"];
                if (json.contains("DepthSorting")) comp.DepthSorting = json["DepthSorting"];

                if (json.contains("Properties"))
                {
                    auto props = json["Properties"];
                    // Helpers for array reading
                    auto getVec3 = [](const nlohmann::json& j) { return glm::vec3(j[0], j[1], j[2]); };
                    auto getVec4 = [](const nlohmann::json& j) { return glm::vec4(j[0], j[1], j[2], j[3]); };

                    comp.Properties.Position = getVec3(props["Position"]);
                    comp.Properties.Velocity = getVec3(props["Velocity"]);
                    comp.Properties.VelocityVariation = getVec3(props["VelocityVariation"]);
                    comp.Properties.ColorBegin = getVec4(props["ColorBegin"]);
                    comp.Properties.ColorEnd = getVec4(props["ColorEnd"]);
                    comp.Properties.SizeBegin = props["SizeBegin"];
                    comp.Properties.SizeEnd = props["SizeEnd"];
                    comp.Properties.SizeVariation = props["SizeVariation"];
                    comp.Properties.LifeTime = props["LifeTime"];
                }
            },
            [](entt::registry& reg, entt::entity entity)
            {
                auto& comp = reg.get<ParticleEmitterComponent>(entity);
                EditorUIHelpers::DrawAssetSelection("Material", comp.Material, {AssetType::Material});

                int maxP = (int)comp.MaxParticles;
                if (ImGui::DragInt("Max Particles", &maxP, 1.0f, 1, 100000))
                {
                    comp.SetMaxParticles(maxP);
                }
                
                ImGui::DragFloat("Emission Rate", &comp.EmissionRate, 0.1f, 0.0f, 1000.0f);
                if (ImGui::Checkbox("Looping", &comp.IsLooping))
                {
                    comp.BurstDone = false;
                }

                if (!comp.IsLooping)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Play Burst"))
                    {
                        comp.BurstDone = false;
                    }
                }

                ImGui::Checkbox("Depth Sorting", &comp.DepthSorting);
                
                if (ImGui::TreeNode("Properties"))
                {
                    ImGui::DragFloat3("Position Offset", &comp.Properties.Position.x, 0.1f);
                    ImGui::DragFloat3("Velocity", &comp.Properties.Velocity.x, 0.1f);
                    ImGui::DragFloat3("Velocity Variation", &comp.Properties.VelocityVariation.x, 0.1f);
                
                    ImGui::ColorEdit4("Color Begin", &comp.Properties.ColorBegin.r);
                    ImGui::ColorEdit4("Color End", &comp.Properties.ColorEnd.r);
                
                    ImGui::DragFloat("Size Begin", &comp.Properties.SizeBegin, 0.1f);
                    ImGui::DragFloat("Size End", &comp.Properties.SizeEnd, 0.1f);
                    ImGui::DragFloat("Size Variation", &comp.Properties.SizeVariation, 0.1f);
                
                    ImGui::DragFloat("Life Time", &comp.Properties.LifeTime, 0.1f, 0.0f, 100.0f);
                
                    ImGui::TreePop();
                }
            });
        m_ComponentRegistry.RegisterCoreComponent<UICanvasComponent>("UICanvas",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& comp = reg.get<UICanvasComponent>(entity);
                if (comp.Canvas)
                {
                    nlohmann::json canvasJson;
                    // Recursively save the entire UI Tree
                    comp.Canvas->Serialize(canvasJson);
                    json["Canvas"] = canvasJson;
                }
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& comp = reg.get_or_emplace<UICanvasComponent>(entity);
                if (json.contains("Canvas"))
                {
                    // Create a new Canvas (which is also the root UIElement)
                    comp.Canvas = std::make_shared<UICanvas>();
                    // Recursively load the tree
                    comp.Canvas->Deserialize(json["Canvas"]);
                }
            }
        );
    }
}
