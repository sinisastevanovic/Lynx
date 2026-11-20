#pragma once
#include "TypeRegistry.h"
#include "Window.h"
#include "Asset/AssetManager.h"
#include "Core/ContentPaths.h"
#include "ImGui/ProfilerOverlay.h"
#include "Profiling/Benchmark.h"
#include "Profiling/FrameProfiler.h"
#include "Project/Project.h"
#include "Renderer/Renderer.h"
#include "Event/Event.h"
#include "Lynx/Renderer/EditorCamera.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/ShaderCache.h"
#include "Scripting/ScriptEngine.h"

namespace Lynx
{
    class IGameModule;
    class EditorCamera;
    class Renderer;
    class Scene;
    class ScriptEngine;

    enum class SceneState
    {
        Edit = 0,
        Play = 1
    };

    class LX_API Engine
    {
    public:
        static Engine& Get() { return *s_Instance; }
        
        void Initialize(const Project& project, bool editorMode = false);
        // Registers the game's components and scripts. Call it before any scene is loaded:
        // a scene drops the components whose types are not registered yet.
        void SetGameModule(IGameModule* gameModule);
        void Run();
        void PreGameShutdown();
        void Shutdown();

        // Turns the session into a benchmark: the game plays the benchmark's scene with a scripted camera,
        // the engine measures a fixed number of frames, writes the results and then leaves Run.
        // Call it after Initialize and before the scene starts playing.
        void SetBenchmark(BenchmarkSettings settings);
        const Benchmark* GetBenchmark() const { return m_Benchmark.get(); }

        FrameProfiler& GetProfiler() { return m_Profiler; }

        // The overlay shows frame, CPU and GPU times on top of the picture. F3 toggles it.
        void SetProfilerOverlayVisible(bool visible) { m_ShowProfilerOverlay = visible; }

        inline Window& GetWindow() { return *m_Window; }
        const Project& GetProject() const { return m_Project; }
        const ContentPaths& GetContentPaths() const { return *m_ContentPaths; }
        const ShaderCache& GetShaderCache() const { return *m_ShaderCache; }
        AssetManager& GetAssetManager() { return *m_AssetManager; }
        AssetRegistry& GetAssetRegistry() { return *m_AssetRegistry; }
        Renderer& GetRenderer() { return *m_Renderer; }
        ScriptEngine* GetScriptEngine() { return m_ScriptEngine.get(); }
        EditorCamera& GetEditorCamera() { return m_EditorCamera; }
        TypeRegistry& GetComponentRegistry() { return m_ComponentRegistry;}
        GameTypeRegistry GetGameRegistry() { return GameTypeRegistry(m_ComponentRegistry); }
        
        void ClearGameTypes();
        
        std::shared_ptr<Scene> GetActiveScene() const { return m_Scene; }
        void ClearActiveScene();
        
        // The engine starts in Edit state. Play begins and ends only through these calls.
        void StartPlay(std::shared_ptr<Scene> runtimeScene);
        void StopPlay(std::shared_ptr<Scene> editorScene);

        // Loads the project's start scene and plays a copy of it. This is how a standalone game,
        // which has no editor to press Play, gets into Play state. Returns false if the scene cannot be loaded.
        // A benchmark that names a scene replaces the start scene.
        bool PlayStartScene();
        void LoadScene(std::shared_ptr<Scene> newScene);
        void LoadScene(const std::filesystem::path& path);

        SceneState GetSceneState() const { return m_SceneState; }

        void SetImGuiRenderCallback(std::function<void()> callback) { m_ImGuiCallback = callback; }

        void SetBlockEvents(bool block) { m_BlockEvents = block; }
        bool AreEventsBlocked() const { return m_BlockEvents; }

        using EventCallbackFn = std::function<void(Event&)>;
        void SetAppEventCallback(const EventCallbackFn& callback) { m_AppEventCallback = callback; }
        
        float GetDeltaTime() const { return m_DeltaTime; }
        float GetUnscaledDeltaTime() const { return m_UnscaledDeltaTime; }
        
        void SetTimeScale(float timeScale) { m_TimeScale = timeScale;}
        float GetTimeScale() const { return m_TimeScale; }
        
        // TODO: We have to differentiate between pausing in game and in editor? 
        void SetPaused(bool paused) { m_Paused = paused; }
        bool IsPaused() const { return m_Paused; }

    private:
        void OnEvent(Event& e);
        
        void SwapActiveScene(std::shared_ptr<Scene> scene);
        void ProcessPendingScene();
        void StartSceneRuntime();
        void EndPlay();
        std::shared_ptr<Scene> m_NextScene = nullptr;

    private:
        static Engine* s_Instance;
        
        Project m_Project;

        // Declared before the asset registry and manager, which hold references to it.
        std::unique_ptr<ContentPaths> m_ContentPaths;
        std::unique_ptr<ShaderCache> m_ShaderCache;
        std::string m_ImGuiLayoutFile;

        // Declared before the renderer, which holds a reference to it.
        FrameProfiler m_Profiler;
        ProfilerOverlay m_ProfilerOverlay;
        bool m_ShowProfilerOverlay = false;
        std::unique_ptr<Benchmark> m_Benchmark;

        std::unique_ptr<Window> m_Window;
        std::unique_ptr<Renderer> m_Renderer;
        std::unique_ptr<AssetManager> m_AssetManager;
        std::unique_ptr<AssetRegistry> m_AssetRegistry;
        std::unique_ptr<ScriptEngine> m_ScriptEngine;
        std::shared_ptr<Scene> m_Scene;
        std::unique_ptr<SceneRenderer> m_SceneRenderer;
        TypeRegistry m_ComponentRegistry;
        
        EditorCamera m_EditorCamera;
        
        bool m_IsRunning = true;

        std::function<void()> m_ImGuiCallback;

        SceneState m_SceneState = SceneState::Edit;

        bool m_BlockEvents = false;
        EventCallbackFn m_AppEventCallback;
        
        IGameModule* m_GameModule = nullptr;
        bool m_IsEditor = false;
        
        float m_TimeScale = 1.0f;
        bool m_Paused = false;
        float m_DeltaTime = 0.0f;
        float m_UnscaledDeltaTime = 0.0f;
        
        friend class EditorLayer;
        friend class AssetManager;
    };
}

