#include "Engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "GameModule.h"
#include "Input.h"

#include "Log.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Scene/Components/Components.h"
#include "Scene/Components/CoreComponentRegistration.h"
#include "Event/KeyEvent.h"
#include "Event/WindowEvent.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "Event/ActionEvent.h"
#include "Event/SceneEvents.h"
#include "Renderer/DebugRenderer.h"


namespace Lynx
{
    Engine* Engine::s_Instance = nullptr;
    
    void Engine::Initialize(const Project& project, bool editorMode)
    {
        s_Instance = this;
        m_IsEditor = editorMode;
        m_Project = project;
        m_ContentPaths = std::make_unique<ContentPaths>(ContentPaths::DefaultEngineResourceDir(), m_Project.RootDirectory, m_Project.AssetDirectory);

        Log::Init(m_ContentPaths->TempDirectory() / "engine.log");
        LX_CORE_INFO("Initializing project '{0}' at {1}", m_Project.Name, m_Project.RootDirectory.string());

        RegisterCoreComponents(m_ComponentRegistry);

        m_Window = Window::Create();
        m_Window->SetEventCallback([this](Event& event){ this->OnEvent(event); });

        m_ShaderCache = std::make_unique<ShaderCache>(m_ContentPaths->TempDirectory() / "ShaderCache");

        m_AssetRegistry = std::make_unique<AssetRegistry>(*m_ContentPaths);
        m_AssetRegistry->LoadRegistry();
        m_AssetManager = std::make_unique<AssetManager>(m_AssetRegistry.get(), *m_ContentPaths);
        
        m_EditorCamera = EditorCamera(45.0f, (float)m_Window->GetWidth() / (float)m_Window->GetHeight(), 0.1f, 1000.0f);

        m_Renderer = std::make_unique<Renderer>(m_Window->GetNativeWindow(), m_Profiler, editorMode);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // ImGui keeps the pointer, so the string has to outlive the context.
        m_ImGuiLayoutFile = (m_ContentPaths->TempDirectory() / "imgui.ini").string();
        io.IniFilename = m_ImGuiLayoutFile.c_str();
        
        ImFontConfig fontConfig;
        fontConfig.GlyphOffset.y = -2.0f;
        const std::string fontPath = m_ContentPaths->EngineResource("Fonts/OpenSans/OpenSans-Regular.ttf").string();
        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, &fontConfig);
        if (font == nullptr)
        {
            LX_CORE_ERROR("Failed to load ImGui font: {0}", fontPath);
            io.Fonts->AddFontDefault();
        }
        
        ImGui_ImplGlfw_InitForVulkan(m_Window->GetNativeWindow(), true);
        //ImGui::StyleColorsDark();
        m_Renderer->Init();

        m_ScriptEngine = std::make_unique<ScriptEngine>();

        m_Scene = std::make_shared<Scene>();
        m_SceneRenderer = std::make_unique<SceneRenderer>(m_Scene);
    }

    void Engine::SetGameModule(IGameModule* gameModule)
    {
        m_GameModule = gameModule;

        GameTypeRegistry gameRegistry(m_ComponentRegistry);
        m_GameModule->RegisterScripts(&gameRegistry);
        m_GameModule->RegisterComponents(&gameRegistry);
    }

    void Engine::SetBenchmark(BenchmarkSettings settings)
    {
        // A benchmark measures the renderer, not the display: its frames must not wait for the refresh.
        m_Renderer->SetVSync(false);
        m_Window->CenterOnPrimaryMonitor(settings.Width, settings.Height);
        m_Benchmark = std::make_unique<Benchmark>(std::move(settings));
    }

    void Engine::Run()
    {
        LX_CORE_INFO("Starting main loop...");
        
        ViewportResizeEvent e(m_Window->GetWidth(), m_Window->GetHeight());
        OnEvent(e);

        auto cubeMesh = m_AssetManager->GetDefaultCube();

        // Load all textures and meshes
        // TODO: This is temporary code, replace with real scene loading logic
        auto view = m_Scene->Reg().view<MeshComponent>();
        for (auto entity : view)
        {
            auto& mesh = view.get<MeshComponent>(entity);
            if (mesh.Mesh.Get())
            {
                for (const auto& subMesh : mesh.Mesh->GetSubmeshes())
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

        const float fixedTimestep = 1.0f / 60.0f;
        const int maxSubSteps = 8;
        float accumulator = 0.0f;
        
        float lastFrameTime = (float)glfwGetTime();
        // This is a placeholder for the real game loop
        while (m_IsRunning)
        {
            m_Profiler.NewFrame();
            ProcessPendingScene();

            float time = (float)glfwGetTime();
            m_UnscaledDeltaTime = time - lastFrameTime;
            lastFrameTime = time;

            m_UnscaledDeltaTime = std::min(m_UnscaledDeltaTime, fixedTimestep * maxSubSteps);

            // A benchmark decides how far each frame advances the game, so that every run renders the same frames.
            if (m_Benchmark && m_SceneState == SceneState::Play)
            {
                m_UnscaledDeltaTime = m_Benchmark->BeginFrame(*m_Scene, *m_Renderer, *m_AssetManager, m_Profiler);
                if (m_Benchmark->IsFinished())
                    m_IsRunning = false;
            }

            m_DeltaTime = m_Paused ? 0.0f : m_UnscaledDeltaTime * m_TimeScale;

            // Fixed steps follow game time, so pausing the game and the time scale apply to physics too.
            if (m_SceneState == SceneState::Play)
                accumulator += m_DeltaTime;
            
            if (m_Window->ShouldClose())
                m_IsRunning = false;
            
            m_Window->OnUpdate();

            auto viewportSize = m_Renderer->GetViewportSize();
            if (m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0 || viewportSize.first == 0 || viewportSize.second == 0)
                continue;

            {
                ProfileScope assetScope(m_Profiler, "Assets");
                m_AssetManager->Update();
            }

            {
                ProfileScope imguiScope(m_Profiler, "ImGui");
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                if (m_ImGuiCallback)
                    m_ImGuiCallback();

                if (m_ShowProfilerOverlay)
                    m_ProfilerOverlay.Draw(m_Profiler.Latest());

                ImGui::Render();
            }

            // The frame is begun here and not earlier: the ImGui callback above may read back the picking buffer,
            // which uses the same command list as the frame.
            const bool isFrameOpen = m_Renderer->BeginFrame();

            // TODO: We should load all the textures and meshes before we run the scene!!!
            if (m_SceneState == SceneState::Edit)
            {
                m_EditorCamera.OnUpdate(m_UnscaledDeltaTime);
                m_Scene->OnUpdateEditor(m_UnscaledDeltaTime, m_EditorCamera.GetPosition());
                
                if (isFrameOpen)
                    m_SceneRenderer->RenderEditor(m_EditorCamera, m_UnscaledDeltaTime);
            }
            else if (m_SceneState == SceneState::Play)
            {
                {
                    ProfileScope updateScope(m_Profiler, "Update");
                    m_Scene->OnUpdateRuntime(m_DeltaTime);
                    if (m_GameModule)
                        m_GameModule->OnUpdate(m_DeltaTime);

                    while (accumulator >= fixedTimestep)
                    {
                        m_Scene->OnFixedUpdate(fixedTimestep);
                        accumulator -= fixedTimestep;
                    }

                    m_Scene->OnLateUpdate(m_DeltaTime);
                }

                float alpha = accumulator / fixedTimestep;
                if (isFrameOpen)
                    m_SceneRenderer->RenderRuntime(m_DeltaTime, alpha);
            }

            // The window is presented whether or not a scene was drawn, so the ImGui layer always shows.
            if (isFrameOpen)
                m_Renderer->EndFrame();
            Input::OnUpdate();
        }

        if (m_SceneState == SceneState::Play)
            EndPlay();
    }

    void Engine::PreGameShutdown()
    {
        ClearActiveScene();
        m_AssetManager->UnloadAllGameAssets();
        m_ScriptEngine.reset();
    }

    void Engine::Shutdown()
    {
        LX_CORE_INFO("Shutting down...");
        if (m_SceneState == SceneState::Play)
            EndPlay();

        // Teardown order matters. Scenes hold Lua objects in their script components, so every scene,
        // including the scene assets cached by the asset manager, has to go before the script engine.
        // Assets hold GPU resources, so the asset manager has to go before the renderer.
        m_SceneRenderer.reset();
        m_NextScene.reset();
        m_Scene.reset();
        m_AssetManager.reset();
        m_ScriptEngine.reset();

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_Renderer.reset();
        Log::Shutdown();
    }

    void Engine::ClearGameTypes()
    {
        m_ComponentRegistry.ClearGameComponents();
        m_ComponentRegistry.ClearGameScripts();
        m_GameModule = nullptr;
    }

    void Engine::ClearActiveScene()
    {
        m_Scene.reset();
        m_Scene = std::make_shared<Scene>();
        SwapActiveScene(m_Scene);
    }

    void Engine::StartPlay(std::shared_ptr<Scene> runtimeScene)
    {
        if (m_SceneState == SceneState::Play)
            return;
        
        m_SceneState = SceneState::Play;
        SwapActiveScene(runtimeScene);

        StartSceneRuntime();
        if (m_GameModule)
            m_GameModule->OnStart();
    }

    void Engine::StopPlay(std::shared_ptr<Scene> editorScene)
    {
        if (m_SceneState == SceneState::Edit)
            return;

        EndPlay();
        SwapActiveScene(editorScene);
    }

    bool Engine::PlayStartScene()
    {
        std::filesystem::path startScene = m_Project.StartScene;
        if (m_Benchmark && !m_Benchmark->Settings().Scene.empty())
            startScene = m_ContentPaths->ProjectAsset(m_Benchmark->Settings().Scene);

        if (startScene.empty())
        {
            LX_CORE_ERROR("Project '{0}' does not name a start scene.", m_Project.Name);
            return false;
        }

        const AssetHandle handle = m_AssetManager->GetAssetHandle(startScene);
        if (!handle.IsValid())
        {
            LX_CORE_ERROR("Start scene of project '{0}' not found: {1}", m_Project.Name, startScene.string());
            return false;
        }

        auto scene = m_AssetManager->GetAsset<Scene>(handle, AssetLoadMode::Blocking);
        StartPlay(SceneSerializer::Duplicate(scene));
        return true;
    }

    void Engine::StartSceneRuntime()
    {
        m_ScriptEngine->OnRuntimeStart(m_Scene.get());
        m_Scene->OnRuntimeStart();
    }

    void Engine::EndPlay()
    {
        if (m_GameModule)
            m_GameModule->OnShutdown();

        m_Scene->OnRuntimeStop();
        m_SceneState = SceneState::Edit;
    }

    void Engine::LoadScene(std::shared_ptr<Scene> newScene)
    {
        if (m_SceneState == SceneState::Play)
        {
            m_NextScene = newScene;
        }
        else
        {
            SwapActiveScene(newScene);
        }
    }

    void Engine::LoadScene(const std::filesystem::path& path)
    {
        auto scene = m_AssetManager->GetAsset<Scene>(path, AssetLoadMode::Blocking);
        if (scene)
        {
            LoadScene(scene);
        }
    }

    void Engine::SwapActiveScene(std::shared_ptr<Scene> scene)
    {
        m_Scene = scene;
        if (m_SceneRenderer)
            m_SceneRenderer->SetScene(m_Scene);
        
        ActiveSceneChangedEvent e(m_Scene);
        OnEvent(e);
    }

    void Engine::ProcessPendingScene()
    {
        if (m_NextScene)
        {
            m_Scene->OnRuntimeStop();
            
            SwapActiveScene(m_NextScene);
            m_NextScene = nullptr;

            StartSceneRuntime();
        }
    }

    void Engine::OnEvent(Event& e)
    {
        m_EditorCamera.OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& event)
        {
            if (event.GetKeyCode() == KeyCode::F3 && event.GetRepeatCount() == 0)
                m_ShowProfilerOverlay = !m_ShowProfilerOverlay;

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
            m_Renderer->EnsureEditorViewport(event.GetWidth(), event.GetHeight());
            m_SceneRenderer->SetViewportSize(event.GetWidth(), event.GetHeight());
            return false;
        });

        if (m_AppEventCallback)
        {
            m_AppEventCallback(e);
            if (e.Handled)
                return;
        }
        if (m_Scene)
        {
            m_Scene->OnEvent(e);
        }
    }
}
