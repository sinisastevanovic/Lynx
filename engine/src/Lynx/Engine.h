#pragma once
#include "TypeRegistry.h"
#include "Window.h"
#include "Asset/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Physics/PhysicsSystem.h"
#include "Event/Event.h"
#include "Lynx/Renderer/EditorCamera.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Systems/ParticleSystem.h"
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
        void SetActiveScene(std::shared_ptr<Scene> scene);
        void ClearActiveScene();

        SceneState GetSceneState() const { return m_SceneState; }
        void SetSceneState(SceneState state);

        void SetImGuiRenderCallback(std::function<void()> callback) { m_ImGuiCallback = callback; }

        void SetBlockEvents(bool block) { m_BlockEvents = block; }
        bool AreEventsBlocked() const { return m_BlockEvents; }

        using EventCallbackFn = std::function<void(Event&)>;
        void SetAppEventCallback(const EventCallbackFn& callback) { m_AppEventCallback = callback; }

    private:
        void OnEvent(Event& e);
        void RegisterCoreScripts();
        void RegisterCoreComponents();

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
        
        friend class EditorLayer;
        friend class AssetManager;
    };
}

