#pragma once
#include "TypeRegistry.h"
#include "Window.h"
#include "Asset/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Event/Event.h"
#include "Lynx/Renderer/EditorCamera.h"
#include "Renderer/SceneRenderer.h"
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
        
        void Initialize(bool editorMode = false);
        void Run(IGameModule* gameModule);
        void Shutdown();

        inline Window& GetWindow() { return *m_Window; }
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
        
        void StartPlay(std::shared_ptr<Scene> runtimeScene);
        void StopPlay(std::shared_ptr<Scene> editorScene);
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
        
        void SetPaused(bool paused) { m_Paused = paused; }
        bool IsPaused() const { return m_Paused; }

    private:
        void OnEvent(Event& e);
        void RegisterCoreScripts();
        void RegisterCoreComponents();
        
        void SwapActiveScene(std::shared_ptr<Scene> scene);
        void ProcessPendingScene();
        std::shared_ptr<Scene> m_NextScene = nullptr;

    private:
        static Engine* s_Instance;
        
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

        SceneState m_SceneState = SceneState::Play;

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

